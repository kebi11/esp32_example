/**
 * @file web_server.h
 * @brief 基于 esp_http_server 的 HTTP 服务与 REST API。
 *
 * 提供的接口见 docs/api.md：
 *   GET /            Web Dashboard
 *   GET /api/status  设备状态
 *   GET /api/gnss    定位信息
 *   GET /api/device  完整信息（system + wifi + gnss）
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 HTTP Server，注册静态页面与 REST handler。
 * @return ESP_OK 成功。
 */
esp_err_t web_server_start(void);

/**
 * @brief 停止 HTTP Server。
 * @return ESP_OK 成功。
 */
esp_err_t web_server_stop(void);

#ifdef __cplusplus
}
#endif
