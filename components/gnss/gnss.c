#include "gnss.h"

#include <string.h>

#include "app_config.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nmea_parser.h"

static const char *TAG = "GNSS";

/* TODO(Phase 1): 安装 UART 驱动 + ring buffer/event queue
 *   uart_param_config(GNSS_UART_NUM, &cfg)
 *   uart_set_pin(GNSS_UART_NUM, GNSS_UART_TX_GPIO, GNSS_UART_RX_GPIO, ...)
 *   uart_driver_install(GNSS_UART_NUM, RX_BUF_SIZE, 0, &queue, ...)
 */

/* TODO(Phase 2): gnss_task 主循环
 *   uart_read_bytes -> 按 '\n' 切行 -> nmea_verify_checksum -> nmea_parse_line
 *   -> 加锁更新 s_data
 */

/* TODO(Phase 2): 静态快照 + 保护。第一版用 mutex，后续可换队列
 *   static gnss_data_t s_data;
 *   static SemaphoreHandle_t s_mutex;
 */

esp_err_t gnss_init(void)
{
    // TODO(Phase 1): 初始化 UART
    ESP_LOGW(TAG, "gnss_init not implemented yet (uart%d, %d baud)",
             GNSS_UART_NUM, GNSS_UART_BAUDRATE);
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gnss_start(void)
{
    // TODO(Phase 1/2): xTaskCreate(gnss_task, ...)
    ESP_LOGW(TAG, "gnss_start not implemented yet");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gnss_get_latest(gnss_data_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // TODO(Phase 2): 在 mutex 保护下拷贝 s_data，当前返回全零快照
    memset(out, 0, sizeof(*out));
    return ESP_ERR_NOT_SUPPORTED;
}
