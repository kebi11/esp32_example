/**
 * @file wifi_manager.h
 * @brief Wi-Fi STA 连接、自动重连与 AP 配网。
 *
 * STA 为主模式；无凭据或重试超限时回退到 AP 模式（Phase 8 配网），
 * 用户通过网页提交凭据后切回 STA。
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Wi-Fi 当前状态 */
typedef struct {
    bool connected;   /**< 是否已连上 AP 并拿到 IP */
    int8_t rssi;      /**< 信号强度 dBm，未连接时为 0 */
    char ip[16];      /**< 点分十进制 IPv4，如 "192.168.1.105" */
    bool ap_mode;     /**< 是否处于 AP 配网模式 */
} wifi_status_t;

/**
 * @brief 初始化 netif / event loop / Wi-Fi 驱动。
 * @return ESP_OK 成功。
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief 启动 STA 连接流程；无凭据时自动转入 AP 配网。
 * @return ESP_OK 成功（已连接或已进入 AP 模式）。
 */
esp_err_t wifi_manager_start(void);

/**
 * @brief 切换到 AP 配网模式，广播 APP_AP_SSID 热点。
 * @return ESP_OK 成功。
 */
esp_err_t wifi_manager_start_ap(void);

/**
 * @brief 保存新凭据到 NVS 并切回 STA 重新连接。
 *
 * @param ssid     新的 Wi-Fi SSID，不可为 NULL。
 * @param password 新的 Wi-Fi 密码，不可为 NULL。
 * @return ESP_OK 成功。
 */
esp_err_t wifi_manager_apply_credentials(const char *ssid, const char *password);

/**
 * @brief 获取当前 Wi-Fi 状态快照。
 *
 * @param[out] out 输出结构体，不可为 NULL。
 * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 参数为 NULL。
 */
esp_err_t wifi_manager_get_status(wifi_status_t *out);

#ifdef __cplusplus
}
#endif
