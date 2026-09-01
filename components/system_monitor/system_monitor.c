/**
 * @file system_monitor.c
 * @brief 运行状态统计：uptime、剩余堆、最小堆。
 *
 * system_monitor_task 周期采样并打印日志、喂任务看门狗（Phase 9）。
 */

#include "system_monitor.h"

#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SYS_MON";

/** 采样日志间隔 */
#define MONITOR_LOG_INTERVAL_MS 10000

static TaskHandle_t s_task_handle = NULL;

static void system_monitor_task(void *arg)
{
    (void)arg;

    esp_task_wdt_add(NULL);

    while (true) {
        esp_task_wdt_reset();

        system_status_t st;
        system_monitor_get_status(&st);
        ESP_LOGI(TAG, "uptime %lu s, heap %lu B, min heap %lu B",
                 (unsigned long)st.uptime_s,
                 (unsigned long)st.free_heap,
                 (unsigned long)st.min_free_heap);

        vTaskDelay(pdMS_TO_TICKS(MONITOR_LOG_INTERVAL_MS));
    }
}

esp_err_t system_monitor_start(void)
{
    if (s_task_handle != NULL) {
        ESP_LOGW(TAG, "system_monitor_task already running");
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(system_monitor_task, "sys_mon", 3072, NULL, 3, &s_task_handle);
    if (ok != pdPASS) {
        s_task_handle = NULL;
        ESP_LOGE(TAG, "failed to create system_monitor_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "system_monitor_task started");
    return ESP_OK;
}

esp_err_t system_monitor_get_status(system_status_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    /* heap / uptime 不依赖后台任务，直接读取 */
    out->free_heap = esp_get_free_heap_size();
    out->min_free_heap = esp_get_minimum_free_heap_size();
    out->uptime_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);

    return ESP_OK;
}
