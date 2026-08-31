#include "wifi_manager.h"

#include <string.h>

#include "app_config.h"
#include "esp_log.h"
#include "storage.h"

static const char *TAG = "WIFI";

/* TODO(Phase 3): esp_netif_init / esp_event_loop_create_default
 *                esp_netif_create_default_wifi_sta
 *                esp_wifi_init + esp_wifi_set_mode(WIFI_MODE_STA)
 * TODO(Phase 3): 注册 WIFI_EVENT / IP_EVENT 回调
 *                - WIFI_EVENT_STA_DISCONNECTED -> 计数重试，超过 WIFI_RETRY_MAX 转 AP
 *                - IP_EVENT_STA_GOT_IP         -> 更新 s_status.ip
 * TODO(Phase 9): 断线后不要重启整机，只做 记录日志 -> 等待 -> 重连
 */

// TODO(Phase 3): 运行期状态，读取时加锁
// static wifi_status_t s_status;

esp_err_t wifi_manager_init(void)
{
    // TODO(Phase 3): netif + event loop + wifi driver 初始化
    ESP_LOGW(TAG, "wifi_manager_init not implemented yet");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t wifi_manager_start(void)
{
    // TODO(Phase 7): 优先从 NVS 读取凭据，读不到再退回 APP_WIFI_SSID
    //   char ssid[32], pass[64];
    //   storage_get_wifi_credentials(ssid, sizeof(ssid), pass, sizeof(pass));
    //   if (ssid[0] == '\0') { snprintf(ssid, sizeof(ssid), "%s", APP_WIFI_SSID); }
    ESP_LOGW(TAG, "wifi_manager_start not implemented yet (compile-time ssid: %s)",
             APP_WIFI_SSID);
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t wifi_manager_get_status(wifi_status_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // TODO(Phase 3): 拷贝运行期状态
    memset(out, 0, sizeof(*out));
    return ESP_ERR_NOT_SUPPORTED;
}
