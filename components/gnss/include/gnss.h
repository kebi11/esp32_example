/**
 * @file gnss.h
 * @brief GNSS 模块对外接口。
 *
 * 本模块负责 UART 接收 NMEA、解析，并对外提供最新的定位快照。
 * 其他模块只能通过 gnss_get_latest() 读取数据，不得访问内部变量。
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 定位快照。
 *
 * 由 gnss_task 写入，由 web_server 等消费者通过 gnss_get_latest() 读取。
 * 使用 mutex 保护（见 gnss.c）。
 */
typedef struct {
    bool fix_valid;      /**< 定位是否有效 */
    double latitude;     /**< 纬度，单位为度，北纬为正 */
    double longitude;    /**< 经度，单位为度，东经为正 */
    float altitude_m;    /**< 海拔，单位米 */
    float speed_kmh;     /**< 地面速度，单位 km/h */
    int satellites;      /**< 参与定位的卫星数量 */
    int year;            /**< UTC 年 */
    int month;           /**< UTC 月 */
    int day;             /**< UTC 日 */
    int hour;            /**< UTC 时 */
    int minute;          /**< UTC 分 */
    int second;          /**< UTC 秒 */
} gnss_data_t;

/** 轨迹环形缓冲最大点数 */
#define GNSS_TRACK_MAX 512

/** 单个轨迹点 */
typedef struct {
    double latitude;   /**< 纬度，十进制度 */
    double longitude;  /**< 经度，十进制度 */
    uint32_t uptime_s; /**< 记录时刻的上电秒数 */
} gnss_track_point_t;

/**
 * @brief 配置并初始化 GNSS 使用的 UART。
 * @return ESP_OK 成功。
 */
esp_err_t gnss_init(void);

/**
 * @brief 创建 gnss_task，开始接收并解析 NMEA。
 * @return ESP_OK 成功。
 */
esp_err_t gnss_start(void);

/**
 * @brief 获取最新定位快照。
 *
 * @param[out] out 输出结构体，不可为 NULL。
 * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 参数为 NULL。
 */
esp_err_t gnss_get_latest(gnss_data_t *out);

/**
 * @brief 获取最近记录的轨迹点（从旧到新）。
 *
 * @param[out] out     输出缓冲，容量 max_points 个点，不可为 NULL。
 * @param max_points   缓冲最多能容纳的点数。
 * @param[out] count   实际返回的点数，不可为 NULL。
 * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 参数为 NULL。
 */
esp_err_t gnss_get_track(gnss_track_point_t *out, int max_points, int *count);

#ifdef __cplusplus
}
#endif
