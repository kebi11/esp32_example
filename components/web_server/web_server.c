/**
 * @file web_server.c
 * @brief 基于 esp_http_server 的 HTTP 服务与 REST API。
 *
 * 路由（详见 docs/api.md）：
 *   GET /api/status  设备状态（device/uptime/heap/wifi/ip/rssi）
 *   GET /api/gnss    定位信息（fix/lat/lon/alt/speed/satellites/utc）
 *   GET /api/device  上述两者的汇总
 *
 * 数据一律通过各模块公开 API 获取（框架文档 §21 的 API 边界原则），
 * 不直接读取其他模块的内部变量。
 */

#include "web_server.h"

#include <stdio.h>

#include "app_config.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "gnss.h"
#include "system_monitor.h"
#include "wifi_manager.h"

static const char *TAG = "WEB";

static httpd_handle_t s_server;

/* 由 CMake EMBED_FILES 生成的固件内嵌静态文件符号（见 data_file_embed_asm.cmake） */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");

/** @brief 发送一段内嵌的静态文件。 */
static esp_err_t web_send_embedded(httpd_req_t *req, const char *type,
                                   const uint8_t *start, const uint8_t *end)
{
    httpd_resp_set_type(req, type);
    return httpd_resp_send(req, (const char *)start, end - start);
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    return web_send_embedded(req, "text/html", index_html_start, index_html_end);
}

static esp_err_t style_get_handler(httpd_req_t *req)
{
    return web_send_embedded(req, "text/css", style_css_start, style_css_end);
}

static esp_err_t appjs_get_handler(httpd_req_t *req)
{
    return web_send_embedded(req, "application/javascript", app_js_start, app_js_end);
}

/** @brief 组装并发送一个 cJSON 对象，发送后释放。 */
static esp_err_t web_send_json(httpd_req_t *req, cJSON *root)
{
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (payload == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "json render failed");
    }

    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    esp_err_t err = httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
    cJSON_free(payload);
    return err;
}

/** @brief GET /api/status —— 设备与 Wi-Fi 状态。 */
static esp_err_t status_get_handler(httpd_req_t *req)
{
    system_status_t sys;
    wifi_status_t wifi;

    bool sys_ok = system_monitor_get_status(&sys) == ESP_OK;
    bool wifi_ok = wifi_manager_get_status(&wifi) == ESP_OK;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "oom");
    }

    cJSON_AddStringToObject(root, "device", APP_DEVICE_NAME);
    cJSON_AddNumberToObject(root, "uptime", sys_ok ? (double)sys.uptime_s : 0);
    cJSON_AddNumberToObject(root, "free_heap", sys_ok ? (double)sys.free_heap : 0);
    cJSON_AddBoolToObject(root, "wifi_connected", wifi_ok && wifi.connected);
    cJSON_AddNumberToObject(root, "wifi_rssi", wifi_ok && wifi.connected ? (double)wifi.rssi : 0);
    cJSON_AddStringToObject(root, "ip", wifi_ok ? wifi.ip : "");

    return web_send_json(req, root);
}

/** @brief 把 gnss_data_t 序列化为 JSON 子对象。 */
static cJSON *gnss_to_json(const gnss_data_t *d)
{
    char utc[32];

    cJSON *obj = cJSON_CreateObject();
    if (obj == NULL) {
        return NULL;
    }

    snprintf(utc, sizeof(utc), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             d->year, d->month, d->day, d->hour, d->minute, d->second);

    cJSON_AddBoolToObject(obj, "fix", d->fix_valid);
    cJSON_AddNumberToObject(obj, "latitude", d->latitude);
    cJSON_AddNumberToObject(obj, "longitude", d->longitude);
    cJSON_AddNumberToObject(obj, "altitude", d->altitude_m);
    cJSON_AddNumberToObject(obj, "speed", d->speed_kmh);
    cJSON_AddNumberToObject(obj, "satellites", d->satellites);
    cJSON_AddStringToObject(obj, "utc", utc);

    return obj;
}

/** @brief GET /api/gnss —— 定位信息。 */
static esp_err_t gnss_get_handler(httpd_req_t *req)
{
    gnss_data_t data;

    if (gnss_get_latest(&data) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "gnss not ready");
    }

    cJSON *root = gnss_to_json(&data);
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "oom");
    }

    return web_send_json(req, root);
}

/** @brief GET /api/device —— system + wifi + gnss 汇总。 */
static esp_err_t device_get_handler(httpd_req_t *req)
{
    system_status_t sys;
    wifi_status_t wifi;
    gnss_data_t gnss;

    bool sys_ok = system_monitor_get_status(&sys) == ESP_OK;
    bool wifi_ok = wifi_manager_get_status(&wifi) == ESP_OK;
    bool gnss_ok = gnss_get_latest(&gnss) == ESP_OK;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "oom");
    }

    cJSON *sys_obj = cJSON_AddObjectToObject(root, "system");
    if (sys_obj != NULL) {
        cJSON_AddStringToObject(sys_obj, "device", APP_DEVICE_NAME);
        cJSON_AddNumberToObject(sys_obj, "uptime", sys_ok ? (double)sys.uptime_s : 0);
        cJSON_AddNumberToObject(sys_obj, "free_heap", sys_ok ? (double)sys.free_heap : 0);
        cJSON_AddNumberToObject(sys_obj, "min_free_heap", sys_ok ? (double)sys.min_free_heap : 0);
    }

    cJSON *wifi_obj = cJSON_AddObjectToObject(root, "wifi");
    if (wifi_obj != NULL) {
        cJSON_AddBoolToObject(wifi_obj, "connected", wifi_ok && wifi.connected);
        cJSON_AddNumberToObject(wifi_obj, "rssi", wifi_ok && wifi.connected ? (double)wifi.rssi : 0);
        cJSON_AddStringToObject(wifi_obj, "ip", wifi_ok ? wifi.ip : "");
    }

    if (gnss_ok) {
        cJSON *gnss_obj = gnss_to_json(&gnss);
        if (gnss_obj != NULL) {
            cJSON_AddItemToObject(root, "gnss", gnss_obj);
        }
    }

    return web_send_json(req, root);
}

esp_err_t web_server_start(void)
{
    if (s_server != NULL) {
        ESP_LOGW(TAG, "server already running");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WEB_SERVER_PORT;
    /* 前端页面 + 静态资源 + 3 个 API，共 6 个路由 */
    config.max_uri_handlers = 8;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        s_server = NULL;
        return err;
    }

    static const httpd_uri_t index_uri = {
        .uri = "/", .method = HTTP_GET, .handler = index_get_handler,
    };
    static const httpd_uri_t style_uri = {
        .uri = "/style.css", .method = HTTP_GET, .handler = style_get_handler,
    };
    static const httpd_uri_t appjs_uri = {
        .uri = "/app.js", .method = HTTP_GET, .handler = appjs_get_handler,
    };
    static const httpd_uri_t status_uri = {
        .uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler,
    };
    static const httpd_uri_t gnss_uri = {
        .uri = "/api/gnss", .method = HTTP_GET, .handler = gnss_get_handler,
    };
    static const httpd_uri_t device_uri = {
        .uri = "/api/device", .method = HTTP_GET, .handler = device_get_handler,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &index_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &style_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &appjs_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &status_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &gnss_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &device_uri));

    ESP_LOGI(TAG, "http server on port %d: / /style.css /app.js /api/status /api/gnss /api/device",
             WEB_SERVER_PORT);
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (s_server == NULL) {
        return ESP_OK;
    }

    esp_err_t err = httpd_stop(s_server);
    s_server = NULL;

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_stop failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "http server stopped");
    return ESP_OK;
}
