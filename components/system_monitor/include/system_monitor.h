/**
 * @file system_monitor.h
 * @brief 运行状态统计：uptime、剩余堆、最小堆、CPU 占用等。
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 系统运行状况快照 */
typedef struct {
    uint32_t uptime_s;       /**< 上电后经过的秒数 */
    uint32_t free_heap;      /**< 当前剩余堆字节数 */
    uint32_t min_free_heap;  /**< 历史最小剩余堆字节数 */
    uint8_t cpu_usage;       /**< 最近一次采样的 CPU 占用百分比，暂未实现时为 0 */
} system_status_t;

/**
 * @brief 创建 system_monitor_task，开始周期采样。
 * @return ESP_OK 成功。
 */
esp_err_t system_monitor_start(void);

/**
 * @brief 获取系统状态快照。
 *
 * @param[out] out 输出结构体，不可为 NULL。
 * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 参数为 NULL。
 */
esp_err_t system_monitor_get_status(system_status_t *out);

#ifdef __cplusplus
}
#endif
