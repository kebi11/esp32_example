#include "storage.h"

#include "esp_log.h"

static const char *TAG = "STORAGE";

/** NVS 命名空间 */
#define STORAGE_NAMESPACE "app_config"
/** NVS 键：Wi-Fi SSID */
#define STORAGE_KEY_WIFI_SSID "wifi_ssid"
/** NVS 键：Wi-Fi 密码 */
#define STORAGE_KEY_WIFI_PASSWORD "wifi_password"

esp_err_t storage_init(void)
{
    // TODO(Phase 7): nvs_flash_init()，遇到 ESP_ERR_NVS_NO_FREE_PAGES /
    // ESP_ERR_NVS_NEW_VERSION_FOUND 时 nvs_flash_erase() 后重试
    ESP_LOGW(TAG, "storage_init not implemented yet");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t storage_get_wifi_credentials(char *ssid, size_t ssid_len,
                                       char *password, size_t password_len)
{
    // TODO(Phase 7): nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &h)
    //                nvs_get_str(h, STORAGE_KEY_WIFI_SSID, ssid, &ssid_len)
    //                nvs_get_str(h, STORAGE_KEY_WIFI_PASSWORD, password, &password_len)
    if (ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ssid_len > 0) {
        ssid[0] = '\0';
    }
    if (password_len > 0) {
        password[0] = '\0';
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t storage_set_wifi_credentials(const char *ssid, const char *password)
{
    // TODO(Phase 7): nvs_open(..., NVS_READWRITE) -> nvs_set_str -> nvs_commit
    if (ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGW(TAG, "storage_set_wifi_credentials not implemented yet");
    return ESP_ERR_NOT_SUPPORTED;
}
