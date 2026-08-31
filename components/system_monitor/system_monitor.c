#include "system_monitor.h"

#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

static const char *TAG = "SYS_MON";

/* TODO(Phase 9): xTaskCreate(system_monitor_task)，周期 1 s 打印
 *   ESP_LOGI(TAG, "uptime %llu s, heap %u, min %u", ...)
 */

esp_err_t system_monitor_start(void)
{
    // TODO(Phase 9): 创建周期采样任务
    ESP_LOGW(TAG, "system_monitor_start not implemented yet");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t system_monitor_get_status(system_status_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    // heap / uptime 不依赖后台任务，先直接读取
    out->free_heap = esp_get_free_heap_size();
    out->min_free_heap = esp_get_minimum_free_heap_size();
    out->uptime_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);

    return ESP_OK;
}
