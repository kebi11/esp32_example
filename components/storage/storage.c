/**
 * @file storage.c
 * @brief NVS 持久化封装。
 *
 * namespace: app_config
 * 键：wifi_ssid / wifi_password（device_name 等后续按需增加）
 *
 * 注意：esp_wifi_init() 内部依赖 NVS（存 RF 校准数据），
 * 所以 storage_init() 必须在 wifi_manager_init() 之前完成。
 */

#include "storage.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "STORAGE";

/** NVS 命名空间 */
#define STORAGE_NAMESPACE "app_config"
/** NVS 键：Wi-Fi SSID */
#define STORAGE_KEY_WIFI_SSID "wifi_ssid"
/** NVS 键：Wi-Fi 密码 */
#define STORAGE_KEY_WIFI_PASSWORD "wifi_password"

esp_err_t storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* 分区布局变化或版本不兼容：擦除后重试（会丢已存配置，属预期行为） */
        ESP_LOGW(TAG, "nvs needs reformat (%s), erasing...", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "nvs ready (namespace \"%s\")", STORAGE_NAMESPACE);
    return ESP_OK;
}

esp_err_t storage_get_wifi_credentials(char *ssid, size_t ssid_len,
                                       char *password, size_t password_len)
{
    if (ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ssid_len > 0) {
        ssid[0] = '\0';
    }
    if (password_len > 0) {
        password[0] = '\0';
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        /* namespace 还没写过，等于没有凭据 */
        return ESP_ERR_NVS_NOT_FOUND;
    }
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_get_str(h, STORAGE_KEY_WIFI_SSID, ssid, &ssid_len);
    if (err == ESP_OK) {
        err = nvs_get_str(h, STORAGE_KEY_WIFI_PASSWORD, password, &password_len);
    }

    nvs_close(h);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ssid[0] = '\0';
        password[0] = '\0';
    }
    return err;
}

esp_err_t storage_set_wifi_credentials(const char *ssid, const char *password)
{
    if (ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(h, STORAGE_KEY_WIFI_SSID, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(h, STORAGE_KEY_WIFI_PASSWORD, password);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }

    nvs_close(h);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save wifi credentials failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "wifi credentials saved");
    return ESP_OK;
}
