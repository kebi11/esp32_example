/**
 * @file app_config.h
 * @brief 项目级配置：把 Kconfig 生成的 CONFIG_* 统一映射为代码内使用的宏。
 *
 * 所有跨模块的默认值都在这里收口，其他组件只包含本头文件，
 * 不要各自再去引用 CONFIG_*。
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- 设备 ---- */

#ifndef CONFIG_APP_DEVICE_NAME
#define CONFIG_APP_DEVICE_NAME "esp32s3-gnss-01"
#endif

/** 设备名，上报给 /api/status 的 device 字段 */
#define APP_DEVICE_NAME CONFIG_APP_DEVICE_NAME

/* ---------------------------------------------------------------- Wi-Fi --- */

#ifndef CONFIG_APP_WIFI_SSID
#define CONFIG_APP_WIFI_SSID ""
#endif

#ifndef CONFIG_APP_WIFI_PASSWORD
#define CONFIG_APP_WIFI_PASSWORD ""
#endif

#ifndef CONFIG_APP_WIFI_RETRY_MAX
#define CONFIG_APP_WIFI_RETRY_MAX 10
#endif

/** 编译期默认 SSID，运行期以 NVS 中保存的值优先 */
#define APP_WIFI_SSID     CONFIG_APP_WIFI_SSID
/** 编译期默认密码，运行期以 NVS 中保存的值优先 */
#define APP_WIFI_PASSWORD CONFIG_APP_WIFI_PASSWORD
/** STA 最大重试次数，超过后转 AP 配网 */
#define WIFI_RETRY_MAX    CONFIG_APP_WIFI_RETRY_MAX

/* ------------------------------------------------------------ HTTP 服务 --- */

#ifndef CONFIG_APP_WEB_SERVER_PORT
#define CONFIG_APP_WEB_SERVER_PORT 80
#endif

#define WEB_SERVER_PORT CONFIG_APP_WEB_SERVER_PORT

/* ---------------------------------------------------------------- GNSS ---- */

#ifndef CONFIG_GNSS_UART_NUM
#define CONFIG_GNSS_UART_NUM 1
#endif

#ifndef CONFIG_GNSS_UART_BAUDRATE
#define CONFIG_GNSS_UART_BAUDRATE 9600
#endif

#ifndef CONFIG_GNSS_UART_RX_GPIO
#define CONFIG_GNSS_UART_RX_GPIO 18
#endif

#ifndef CONFIG_GNSS_UART_TX_GPIO
#define CONFIG_GNSS_UART_TX_GPIO 17
#endif

#ifndef CONFIG_GNSS_TASK_STACK_SIZE
#define CONFIG_GNSS_TASK_STACK_SIZE 4096
#endif

#ifndef CONFIG_GNSS_TASK_PRIORITY
#define CONFIG_GNSS_TASK_PRIORITY 5
#endif

/** GNSS 使用的 UART 端口（UART_NUM_0/1/2） */
#define GNSS_UART_NUM         CONFIG_GNSS_UART_NUM
/** GNSS 波特率，S1216F8-BD 默认 9600 */
#define GNSS_UART_BAUDRATE    CONFIG_GNSS_UART_BAUDRATE
/** ESP32 接收脚，接 GNSS TXD */
#define GNSS_UART_RX_GPIO     CONFIG_GNSS_UART_RX_GPIO
/** ESP32 发送脚，接 GNSS RXD（仅配置模块时需要） */
#define GNSS_UART_TX_GPIO     CONFIG_GNSS_UART_TX_GPIO
/** gnss_task 栈大小 */
#define GNSS_TASK_STACK_SIZE  CONFIG_GNSS_TASK_STACK_SIZE
/** gnss_task 优先级 */
#define GNSS_TASK_PRIORITY    CONFIG_GNSS_TASK_PRIORITY

/**
 * @brief 打印当前生效的配置，便于上电时确认编译参数。
 *
 * @return ESP_OK 始终成功。
 */
esp_err_t app_config_init(void);

#ifdef __cplusplus
}
#endif
