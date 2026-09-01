/**
 * @file gnss.c
 * @brief GNSS UART 接收、NMEA 解析、轨迹记录与异常恢复。
 *
 * UART 收到的每行 NMEA 经 nmea_parse_line() 校验解析后，合并进 mutex 保护的
 * 共享快照 s_data，外部通过 gnss_get_latest() 读取；有效定位按 1 Hz 降采样
 * 记入环形轨迹缓冲，通过 gnss_get_track() 读取。
 * UART 读错误或长时间无数据时自动重装驱动恢复（Phase 9 可靠性）。
 */

#include "gnss.h"

#include <stdbool.h>
#include <string.h>

#include "app_config.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nmea_parser.h"

static const char *TAG = "GNSS";

/** UART 驱动接收环形缓冲区大小，约可缓存 10 条 NMEA 语句 */
#define GNSS_UART_RX_BUF_SIZE 1024
/** 只收不发，TX 缓冲区为 0；下发配置命令时再调大 */
#define GNSS_UART_TX_BUF_SIZE 0
/** 单次 uart_read_bytes 的最大读取长度 */
#define GNSS_UART_RX_CHUNK    128
/** 读取超时，同时决定空闲时任务的唤醒频率 */
#define GNSS_UART_READ_TIMEOUT_MS 100
/** 连续多久没收到数据就告警一次，用于排查接线和波特率 */
#define GNSS_NO_DATA_WARN_MS  5000
/** 连续读错误达到该次数后重装 UART 驱动恢复 */
#define GNSS_UART_ERR_LIMIT   10
/** 连续无数据达到该时长后重装 UART 驱动恢复 */
#define GNSS_STALL_RECOVER_MS 60000
/** 摘要日志打印间隔 */
#define GNSS_SUMMARY_INTERVAL_MS 1000
/** 轨迹点降采样间隔（秒） */
#define GNSS_TRACK_INTERVAL_S 1

static TaskHandle_t s_task_handle = NULL;

/* 共享定位快照：gnss_task 写，web_server 等读，mutex 保护 */
static gnss_data_t s_data;
static SemaphoreHandle_t s_data_mutex;

/* 轨迹环形缓冲（RAM，断电不保留） */
static gnss_track_point_t s_track[GNSS_TRACK_MAX];
static int s_track_count;
static uint32_t s_last_track_s;

/** @brief 配置并安装 UART 驱动（初始化与异常恢复共用）。 */
static esp_err_t gnss_uart_init_hw(void)
{
    const uart_config_t uart_cfg = {
        .baud_rate  = GNSS_UART_BAUDRATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config((uart_port_t)GNSS_UART_NUM, &uart_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_set_pin((uart_port_t)GNSS_UART_NUM, GNSS_UART_TX_GPIO, GNSS_UART_RX_GPIO,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_driver_install((uart_port_t)GNSS_UART_NUM, GNSS_UART_RX_BUF_SIZE,
                              GNSS_UART_TX_BUF_SIZE, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

/** @brief 卸载并重装 UART 驱动，从异常状态恢复。 */
static void gnss_uart_recover(void)
{
    ESP_LOGW(TAG, "recovering uart%d driver...", GNSS_UART_NUM);
    uart_driver_delete((uart_port_t)GNSS_UART_NUM);
    if (gnss_uart_init_hw() == ESP_OK) {
        ESP_LOGI(TAG, "uart%d recovered", GNSS_UART_NUM);
    } else {
        ESP_LOGE(TAG, "uart%d recovery failed", GNSS_UART_NUM);
    }
}

/** @brief 有效定位时按 1 Hz 降采样追加轨迹点（环形）。 */
static void gnss_track_append(const gnss_data_t *d)
{
    uint32_t now_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);

    if (now_s == s_last_track_s) {
        return; /* 降采样：每秒最多一点 */
    }
    s_last_track_s = now_s;

    int idx = s_track_count % GNSS_TRACK_MAX;
    s_track[idx].latitude = d->latitude;
    s_track[idx].longitude = d->longitude;
    s_track[idx].uptime_s = now_s;
    s_track_count++;
}

/**
 * @brief 处理一条完整的 NMEA 行：校验、解析、合并进快照。
 */
static void gnss_handle_line(const char *line)
{
    /* 取当前快照，解析结果按字段合并（RMC/GGA 只更新各自包含的字段） */
    gnss_data_t snapshot;

    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGD(TAG, "snapshot lock busy, drop line");
        return;
    }
    snapshot = s_data;
    xSemaphoreGive(s_data_mutex);

    esp_err_t err = nmea_parse_line(line, &snapshot);
    if (err == ESP_ERR_NOT_FOUND) {
        return; /* 本版不处理的语句类型（GSA/GSV/VTG...） */
    }
    if (err == ESP_ERR_INVALID_CRC) {
        ESP_LOGW(TAG, "checksum error, dropping: %s", line);
        return;
    }
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "parse failed (%s): %s", esp_err_to_name(err), line);
        return;
    }

    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGD(TAG, "snapshot lock busy, drop update");
        return;
    }
    s_data = snapshot;
    xSemaphoreGive(s_data_mutex);

    /* 有效定位时记录轨迹 */
    if (snapshot.fix_valid) {
        gnss_track_append(&snapshot);
    }

    /* 原始行输出，需要时用 idf.py monitor --log-level DEBUG 打开 */
    ESP_LOGD(TAG, "%s", line);
}

/** @brief 打印一次定位摘要。 */
static void gnss_log_summary(void)
{
    gnss_data_t d;

    if (gnss_get_latest(&d) != ESP_OK) {
        return;
    }

    ESP_LOGI(TAG,
             "fix=%s sats=%d lat=%.6f lon=%.6f alt=%.1fm speed=%.1fkm/h "
             "utc=%04d-%02d-%02d %02d:%02d:%02d",
             d.fix_valid ? "YES" : "NO ", d.satellites, d.latitude, d.longitude,
             d.altitude_m, d.speed_kmh, d.year, d.month, d.day,
             d.hour, d.minute, d.second);
}

/**
 * @brief UART 接收任务：读字节 -> 按 '\n' 切行 -> 解析，含异常恢复与看门狗。
 */
static void gnss_task(void *arg)
{
    uint8_t rx_buf[GNSS_UART_RX_CHUNK];
    char line[NMEA_LINE_MAX_LEN];
    size_t line_len = 0;
    int idle_ms = 0;
    int err_count = 0;
    bool warned_no_data = false;
    int64_t last_summary_us = esp_timer_get_time();

    (void)arg;

    /* 订阅任务看门狗：本任务循环阻塞最多 100 ms，正常不会超时 */
    esp_task_wdt_add(NULL);

    while (true) {
        esp_task_wdt_reset();

        int n = uart_read_bytes((uart_port_t)GNSS_UART_NUM, rx_buf, sizeof(rx_buf),
                                pdMS_TO_TICKS(GNSS_UART_READ_TIMEOUT_MS));
        if (n < 0) {
            err_count++;
            if (err_count >= GNSS_UART_ERR_LIMIT) {
                ESP_LOGE(TAG, "uart read error x%d, recovering driver", err_count);
                err_count = 0;
                gnss_uart_recover();
            }
            vTaskDelay(pdMS_TO_TICKS(GNSS_UART_READ_TIMEOUT_MS));
            continue;
        }

        err_count = 0;

        if (n == 0) {
            idle_ms += GNSS_UART_READ_TIMEOUT_MS;
            if (idle_ms >= GNSS_NO_DATA_WARN_MS && !warned_no_data) {
                ESP_LOGW(TAG,
                         "no data for %d ms: check wiring (GNSS TXD -> GPIO%d) and baud rate %d",
                         idle_ms, GNSS_UART_RX_GPIO, GNSS_UART_BAUDRATE);
                warned_no_data = true;
            }
            if (idle_ms >= GNSS_STALL_RECOVER_MS) {
                ESP_LOGW(TAG, "stalled %d ms, recovering driver", idle_ms);
                idle_ms = 0;
                warned_no_data = false;
                line_len = 0;
                gnss_uart_recover();
            }
        } else {
            idle_ms = 0;
            warned_no_data = false;

            for (int i = 0; i < n; i++) {
                char c = (char)rx_buf[i];

                /* NMEA 以 CRLF 结尾，CR 直接丢弃 */
                if (c == '\r') {
                    continue;
                }

                if (c != '\n') {
                    if (line_len < sizeof(line) - 1) {
                        line[line_len++] = c;
                    } else {
                        /* 超长说明中途丢了换行，复位重新同步 */
                        ESP_LOGW(TAG, "line overflow, resync");
                        line_len = 0;
                    }
                    continue;
                }

                if (line_len > 0) {
                    line[line_len] = '\0';
                    gnss_handle_line(line);
                    line_len = 0;
                }
            }
        }

        /* 每秒打印一次摘要 */
        int64_t now = esp_timer_get_time();
        if (now - last_summary_us >= GNSS_SUMMARY_INTERVAL_MS * 1000LL) {
            last_summary_us = now;
            gnss_log_summary();
        }
    }
}

esp_err_t gnss_init(void)
{
    s_data_mutex = xSemaphoreCreateMutex();
    if (s_data_mutex == NULL) {
        ESP_LOGE(TAG, "failed to create data mutex");
        return ESP_FAIL;
    }
    memset(&s_data, 0, sizeof(s_data));
    s_track_count = 0;
    s_last_track_s = 0;

    esp_err_t err = gnss_uart_init_hw();
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "uart%d ready: %d baud, rx gpio %d, tx gpio %d",
             GNSS_UART_NUM, GNSS_UART_BAUDRATE, GNSS_UART_RX_GPIO, GNSS_UART_TX_GPIO);
    return ESP_OK;
}

esp_err_t gnss_start(void)
{
    if (s_task_handle != NULL) {
        ESP_LOGW(TAG, "gnss_task already running");
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(gnss_task, "gnss_task", GNSS_TASK_STACK_SIZE, NULL,
                                GNSS_TASK_PRIORITY, &s_task_handle);
    if (ok != pdPASS) {
        s_task_handle = NULL;
        ESP_LOGE(TAG, "failed to create gnss_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "gnss_task started (stack %d, prio %d)",
             GNSS_TASK_STACK_SIZE, GNSS_TASK_PRIORITY);
    return ESP_OK;
}

esp_err_t gnss_get_latest(gnss_data_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_data_mutex == NULL) {
        memset(out, 0, sizeof(*out));
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    *out = s_data;
    xSemaphoreGive(s_data_mutex);
    return ESP_OK;
}

esp_err_t gnss_get_track(gnss_track_point_t *out, int max_points, int *count)
{
    if (out == NULL || count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int total = s_track_count;
    int n = total < GNSS_TRACK_MAX ? total : GNSS_TRACK_MAX;
    if (n > max_points) {
        n = max_points;
    }

    /* 最新 n 个点，按时间从旧到新输出 */
    int start = (s_track_count - n) % GNSS_TRACK_MAX;
    for (int i = 0; i < n; i++) {
        out[i] = s_track[(start + i) % GNSS_TRACK_MAX];
    }
    *count = n;

    return ESP_OK;
}
