/**
 * @file wifi_manager.h
 * @brief Wi-Fi STA 连接与重连管理。
 *
 * 第一版只做 STA：连接路由器并在断线后自动重连。
 * 第二版（Phase 8）在此基础上加 AP 配网。
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
} wifi_status_t;

/**
 * @brief 初始化 netif / event loop / Wi-Fi 驱动。
 * @return ESP_OK 成功。
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief 启动 STA 连接流程。
 * @return ESP_OK 成功。
 */
esp_err_t wifi_manager_start(void);

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
