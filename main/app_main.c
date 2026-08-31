/**
 * @file app_main.c
 * @brief 只负责系统启动顺序，不写业务逻辑。
 *
 * 顺序：NVS -> 配置 -> GNSS -> Wi-Fi -> HTTP -> 监控
 * 骨架阶段各模块多为占位实现，失败只打 warning 不阻塞启动。
 */

#include "app_config.h"
#include "esp_log.h"
#include "esp_system.h"
#include "gnss.h"
#include "storage.h"
#include "system_monitor.h"
#include "web_server.h"
#include "wifi_manager.h"

static const char *TAG = "APP";

/**
 * @brief 执行一个初始化步骤，失败时记录 warning 而不中止启动。
 */
#define RUN_STEP(fn)                                          \
    do {                                                      \
        esp_err_t __err = (fn)();                             \
        if (__err != ESP_OK) {                                \
            ESP_LOGW(TAG, "%s not ready: %s", #fn,            \
                     esp_err_to_name(__err));                 \
        }                                                     \
    } while (0)

void app_main(void)
{
    ESP_LOGI(TAG, "=== %s boot ===", APP_DEVICE_NAME);
    ESP_LOGI(TAG, "idf version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "chip: %s, heap: %lu bytes", CONFIG_IDF_TARGET,
             (unsigned long)esp_get_free_heap_size());

    /* 存储与配置 */
    RUN_STEP(storage_init);
    RUN_STEP(app_config_init);

    /* GNSS：UART 接收 + NMEA 解析 */
    RUN_STEP(gnss_init);
    RUN_STEP(gnss_start);

    /* 网络：STA 连接与重连 */
    RUN_STEP(wifi_manager_init);
    RUN_STEP(wifi_manager_start);

    /* 对外服务 */
    RUN_STEP(web_server_start);
    RUN_STEP(system_monitor_start);

    ESP_LOGI(TAG, "skeleton ready, core chain:");
    ESP_LOGI(TAG, "  GNSS -> UART -> ESP32-S3 -> Wi-Fi -> HTTP -> Browser");
}
