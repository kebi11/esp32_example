/**
 * @file storage.h
 * @brief NVS 持久化封装。
 *
 * namespace: app_config
 * 已规划键：wifi_ssid / wifi_password / device_name / server_url / mqtt_host
 * 第一版只需要 wifi_ssid / wifi_password。
 */

#pragma once

#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** SSID 缓冲区建议大小 */
#define STORAGE_SSID_MAX_LEN     32
/** WPA2 密码缓冲区建议大小 */
#define STORAGE_PASSWORD_MAX_LEN 64

/**
 * @brief 初始化 NVS Flash。
 *
 * 若分区被截断或版本不兼容，会自动擦除后重新初始化。
 *
 * @return ESP_OK 成功。
 */
esp_err_t storage_init(void);

/**
 * @brief 读取保存的 Wi-Fi 凭据。
 *
 * @param[out] ssid     SSID 缓冲区，建议 32 字节。
 * @param ssid_len      ssid 缓冲区大小。
 * @param[out] password 密码缓冲区，建议 64 字节。
 * @param password_len  password 缓冲区大小。
 * @return ESP_OK 读取成功；
 *         ESP_ERR_NVS_NOT_FOUND 尚未保存过；
 *         ESP_ERR_INVALID_ARG 缓冲区为 NULL。
 */
esp_err_t storage_get_wifi_credentials(char *ssid, size_t ssid_len,
                                       char *password, size_t password_len);

/**
 * @brief 写入 Wi-Fi 凭据，断电后保留。
 *
 * @param ssid     SSID，不可为 NULL。
 * @param password 密码，不可为 NULL。
 * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 参数为 NULL。
 */
esp_err_t storage_set_wifi_credentials(const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif
