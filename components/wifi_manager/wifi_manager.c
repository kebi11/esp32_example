/**
 * @file wifi_manager.c
 * @brief Wi-Fi STA 连接与自动重连。
 *
 * 凭据来源：优先 NVS（storage 模块），读不到回退到编译期默认值（menuconfig 配置）。
 * 断线处理：按框架文档 §15，记录日志 -> 等待 -> 重连，不重启整机。
 * 重试超过 WIFI_RETRY_MAX 后停止主动重连（Phase 8 转 AP 配网）。
 */

#include "wifi_manager.h"

#include <string.h>

#include "app_config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "storage.h"

static const char *TAG = "WIFI";

/** 凭据最大长度，与 storage.h 的建议值一致 */
#define WIFI_SSID_BUF_LEN     STORAGE_SSID_MAX_LEN
#define WIFI_PASSWORD_BUF_LEN STORAGE_PASSWORD_MAX_LEN

/** 已拿到 IP 的事件位，供 wifi_manager_start() 同步等待 */
#define WIFI_CONNECTED_BIT BIT0

static wifi_status_t s_status;
static SemaphoreHandle_t s_status_mutex;
static EventGroupHandle_t s_wifi_events;
static int s_retry_count;

/** @brief STA netif 句柄，断线重连时不再重复创建。 */
static esp_netif_t *s_sta_netif;

/** @brief 在锁保护下更新状态。 */
static void wifi_set_status(bool connected, int8_t rssi, const char *ip)
{
    if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    s_status.connected = connected;
    s_status.rssi = rssi;
    if (ip != NULL) {
        snprintf(s_status.ip, sizeof(s_status.ip), "%s", ip);
    } else if (!connected) {
        s_status.ip[0] = '\0';
    }
    xSemaphoreGive(s_status_mutex);
}

/** @brief 事件回调：处理 STA 生命周期与 IP 事件。 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "sta started, connecting...");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "associated with ap");
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            s_retry_count++;
            wifi_set_status(false, 0, NULL);

            if (s_retry_count <= WIFI_RETRY_MAX) {
                ESP_LOGW(TAG, "disconnected, retry %d/%d", s_retry_count, WIFI_RETRY_MAX);
                esp_wifi_connect();
            } else {
                ESP_LOGE(TAG, "retry limit reached, giving up (AP provisioning in Phase 8)");
            }
            break;
        }

        default:
            break;
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        char ip_str[16];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));

        s_retry_count = 0;

        /* RSSI 此时已可查询 */
        wifi_ap_record_t ap;
        int8_t rssi = 0;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            rssi = ap.rssi;
        }

        wifi_set_status(true, rssi, ip_str);
        if (s_wifi_events != NULL) {
            xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
        }
        ESP_LOGI(TAG, "got ip: %s (rssi %d dBm)", ip_str, rssi);
    }
}

esp_err_t wifi_manager_init(void)
{
    if (s_sta_netif != NULL) {
        return ESP_OK; /* 已初始化 */
    }

    s_status_mutex = xSemaphoreCreateMutex();
    s_wifi_events = xEventGroupCreate();
    if (s_status_mutex == NULL || s_wifi_events == NULL) {
        ESP_LOGE(TAG, "failed to create sync primitives");
        return ESP_FAIL;
    }
    memset(&s_status, 0, sizeof(s_status));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_LOGI(TAG, "wifi driver initialized in sta mode");
    return ESP_OK;
}

esp_err_t wifi_manager_start(void)
{
    if (s_sta_netif == NULL) {
        ESP_LOGE(TAG, "not initialized, call wifi_manager_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    /* 凭据：NVS 优先，回退编译期默认值 */
    char ssid[WIFI_SSID_BUF_LEN] = {0};
    char password[WIFI_PASSWORD_BUF_LEN] = {0};
    bool from_nvs = false;

    if (storage_get_wifi_credentials(ssid, sizeof(ssid), password, sizeof(password))
            == ESP_OK && ssid[0] != '\0') {
        from_nvs = true;
    } else {
        snprintf(ssid, sizeof(ssid), "%s", APP_WIFI_SSID);
        snprintf(password, sizeof(password), "%s", APP_WIFI_PASSWORD);
    }

    if (ssid[0] == '\0') {
        ESP_LOGW(TAG, "no wifi credentials (nvs %s, compile-time empty); "
                 "sta not started, waiting for provisioning",
                 from_nvs ? "set" : "unset");
        return ESP_ERR_INVALID_STATE;
    }

    wifi_config_t wifi_cfg = {0};
    strlcpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
    strlcpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    /* 声明支持 PMF，兼容 WPA3 过渡模式（WPA2/WPA3 混合）的路由器 */
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "connecting to \"%s\" (credentials from %s)",
             ssid, from_nvs ? "nvs" : "compile-time default");

    /* 同步等第一个 IP（断线重连由事件回调异步处理） */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdTRUE, pdMS_TO_TICKS(15000));
    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "no ip within 15 s, continuing in background");
    return ESP_ERR_TIMEOUT;
}

esp_err_t wifi_manager_get_status(wifi_status_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_status_mutex == NULL) {
        memset(out, 0, sizeof(*out));
        return ESP_ERR_INVALID_STATE;
    }

    /* 已连接时顺手刷新 RSSI；查询失败不影响返回旧值 */
    wifi_ap_record_t ap;
    if (s_status.connected && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        wifi_set_status(true, ap.rssi, NULL);
    }

    if (xSemaphoreTake(s_status_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    *out = s_status;
    xSemaphoreGive(s_status_mutex);
    return ESP_OK;
}
