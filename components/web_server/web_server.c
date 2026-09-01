/**
 * @file web_server.c
 * @brief 基于 esp_http_server 的 HTTP 服务、REST API、SSE 推送与静态页面。
 *
 * 路由（详见 docs/api.md）：
 *   GET  /              Web Dashboard
 *   GET  /style.css /app.js /manifest.json /sw.js /icon.svg  静态资源
 *   GET  /api/status   设备状态
 *   GET  /api/gnss     定位信息
 *   GET  /api/device   汇总（system + wifi + gnss）
 *   GET  /api/track    轨迹
 *   GET  /api/events   SSE 实时推送
 *   POST /api/wifi     AP 配网保存凭据
 *
 * 数据一律通过各模块公开 API 获取，不直接读取其他模块内部变量。
 */

#include "web_server.h"

#include <stdio.h>
#include <stdlib.h>

#include "app_config.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
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
extern const uint8_t manifest_json_start[] asm("_binary_manifest_json_start");
extern const uint8_t manifest_json_end[] asm("_binary_manifest_json_end");
extern const uint8_t sw_js_start[] asm("_binary_sw_js_start");
extern const uint8_t sw_js_end[] asm("_binary_sw_js_end");
extern const uint8_t icon_svg_start[] asm("_binary_icon_svg_start");
extern const uint8_t icon_svg_end[] asm("_binary_icon_svg_end");

/* SSE 状态：单客户端广播 */
static SemaphoreHandle_t s_sse_mutex;
static SemaphoreHandle_t s_sse_client_sem;
static httpd_req_t *s_sse_req;
static bool s_sse_active;

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

static esp_err_t manifest_get_handler(httpd_req_t *req)
{
    return web_send_embedded(req, "application/manifest+json",
                             manifest_json_start, manifest_json_end);
}

static esp_err_t swjs_get_handler(httpd_req_t *req)
{
    return web_send_embedded(req, "application/javascript", sw_js_start, sw_js_end);
}

static esp_err_t icon_get_handler(httpd_req_t *req)
{
    return web_send_embedded(req, "image/svg+xml", icon_svg_start, icon_svg_end);
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

/** @brief 构建 /api/status 对应的 JSON 对象。 */
static cJSON *web_status_json(void)
{
    system_status_t sys;
    wifi_status_t wifi;

    bool sys_ok = system_monitor_get_status(&sys) == ESP_OK;
    bool wifi_ok = wifi_manager_get_status(&wifi) == ESP_OK;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    cJSON_AddStringToObject(root, "device", APP_DEVICE_NAME);
    cJSON_AddNumberToObject(root, "uptime", sys_ok ? (double)sys.uptime_s : 0);
    cJSON_AddNumberToObject(root, "free_heap", sys_ok ? (double)sys.free_heap : 0);
    cJSON_AddBoolToObject(root, "wifi_connected", wifi_ok && wifi.connected);
    cJSON_AddBoolToObject(root, "ap_mode", wifi_ok && wifi.ap_mode);
    cJSON_AddNumberToObject(root, "wifi_rssi", wifi_ok && wifi.connected ? (double)wifi.rssi : 0);
    cJSON_AddStringToObject(root, "ip", wifi_ok ? wifi.ip : "");

    return root;
}

/** @brief 把 gnss_data_t 序列化为 JSON 对象。 */
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

/** @brief GET /api/status —— 设备与 Wi-Fi 状态。 */
static esp_err_t status_get_handler(httpd_req_t *req)
{
    cJSON *root = web_status_json();
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "oom");
    }
    return web_send_json(req, root);
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
        cJSON_AddBoolToObject(wifi_obj, "ap_mode", wifi_ok && wifi.ap_mode);
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

/** @brief GET /api/track —— 轨迹点数组。 */
static esp_err_t track_get_handler(httpd_req_t *req)
{
    gnss_track_point_t *points = malloc(GNSS_TRACK_MAX * sizeof(*points));
    if (points == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "oom");
    }

    int count = 0;
    gnss_get_track(points, GNSS_TRACK_MAX, &count);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        free(points);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "oom");
    }

    cJSON *arr = cJSON_AddArrayToObject(root, "track");
    for (int i = 0; i < count; i++) {
        cJSON *p = cJSON_CreateObject();
        if (p == NULL) {
            break;
        }
        cJSON_AddNumberToObject(p, "lat", points[i].latitude);
        cJSON_AddNumberToObject(p, "lon", points[i].longitude);
        cJSON_AddNumberToObject(p, "t", points[i].uptime_s);
        cJSON_AddItemToArray(arr, p);
    }

    free(points);
    return web_send_json(req, root);
}

/** @brief POST /api/wifi —— 接收 {ssid, password}，保存凭据并切回 STA 重连。 */
static esp_err_t wifi_post_handler(httpd_req_t *req)
{
    char buf[256] = {0};
    int received = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (received <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
    }
    buf[received] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
    }

    const cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    const cJSON *password = cJSON_GetObjectItem(root, "password");
    if (!cJSON_IsString(ssid) || !cJSON_IsString(password)) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ssid/password required");
    }

    esp_err_t err = wifi_manager_apply_credentials(ssid->valuestring, password->valuestring);
    cJSON_Delete(root);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "apply credentials failed: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "apply failed");
    }

    cJSON *resp = cJSON_CreateObject();
    if (resp == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "oom");
    }
    cJSON_AddBoolToObject(resp, "ok", true);
    return web_send_json(req, resp);
}

/* ------------------------------------------------------------------ SSE --- */

/** @brief SSE 广播任务：等待客户端接入后每秒推送一次状态。 */
static void sse_broadcast_task(void *arg)
{
    (void)arg;

    while (true) {
        xSemaphoreTake(s_sse_client_sem, portMAX_DELAY);

        httpd_req_t *req = NULL;
        xSemaphoreTake(s_sse_mutex, portMAX_DELAY);
        req = s_sse_req;
        xSemaphoreGive(s_sse_mutex);

        if (req == NULL) {
            continue;
        }

        /* 持续推送，直到发送失败（客户端断开） */
        while (true) {
            gnss_data_t gnss;
            gnss_get_latest(&gnss);

            cJSON *root = cJSON_CreateObject();
            cJSON *status = web_status_json();
            cJSON *gnss_obj = gnss_to_json(&gnss);

            if (root != NULL && status != NULL && gnss_obj != NULL) {
                cJSON_AddItemToObject(root, "status", status);
                cJSON_AddItemToObject(root, "gnss", gnss_obj);
            } else {
                if (status != NULL) {
                    cJSON_Delete(status);
                }
                if (gnss_obj != NULL) {
                    cJSON_Delete(gnss_obj);
                }
            }

            char *payload = root ? cJSON_PrintUnformatted(root) : NULL;
            cJSON_Delete(root);

            char frame[640];
            int len = snprintf(frame, sizeof(frame), "data: %s\n\n",
                               payload != NULL ? payload : "{}");
            if (payload != NULL) {
                cJSON_free(payload);
            }

            if (httpd_resp_send_chunk(req, frame, len) != ESP_OK) {
                break; /* 客户端断开 */
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        /* 清理 */
        httpd_req_async_handler_complete(req);
        xSemaphoreTake(s_sse_mutex, portMAX_DELAY);
        if (s_sse_req == req) {
            s_sse_req = NULL;
        }
        s_sse_active = false;
        xSemaphoreGive(s_sse_mutex);
    }
}

/** @brief GET /api/events —— SSE 入口，把请求转交给广播任务。 */
static esp_err_t events_get_handler(httpd_req_t *req)
{
    xSemaphoreTake(s_sse_mutex, portMAX_DELAY);
    if (s_sse_active) {
        xSemaphoreGive(s_sse_mutex);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "sse busy");
    }
    s_sse_active = true;
    xSemaphoreGive(s_sse_mutex);

    httpd_req_t *async_req = NULL;
    if (httpd_req_async_handler_begin(req, &async_req) != ESP_OK) {
        xSemaphoreTake(s_sse_mutex, portMAX_DELAY);
        s_sse_active = false;
        xSemaphoreGive(s_sse_mutex);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "async begin failed");
    }

    httpd_resp_set_type(async_req, "text/event-stream");
    httpd_resp_set_hdr(async_req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(async_req, "Connection", "keep-alive");

    xSemaphoreTake(s_sse_mutex, portMAX_DELAY);
    s_sse_req = async_req;
    xSemaphoreGive(s_sse_mutex);

    xSemaphoreGive(s_sse_client_sem);
    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    if (s_server != NULL) {
        ESP_LOGW(TAG, "server already running");
        return ESP_OK;
    }

    s_sse_mutex = xSemaphoreCreateMutex();
    s_sse_client_sem = xSemaphoreCreateBinary();
    if (s_sse_mutex == NULL || s_sse_client_sem == NULL) {
        ESP_LOGE(TAG, "failed to create sse sync primitives");
        return ESP_FAIL;
    }

    BaseType_t ok = xTaskCreate(sse_broadcast_task, "sse", 4096, NULL, 5, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "failed to create sse_broadcast_task");
        return ESP_FAIL;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WEB_SERVER_PORT;
    /* 静态 6 + API 6，共 12 个路由，留余量 */
    config.max_uri_handlers = 16;
    /* SSE 用长连接，允许更多 socket */
    config.max_open_sockets = 7;

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
    static const httpd_uri_t manifest_uri = {
        .uri = "/manifest.json", .method = HTTP_GET, .handler = manifest_get_handler,
    };
    static const httpd_uri_t swjs_uri = {
        .uri = "/sw.js", .method = HTTP_GET, .handler = swjs_get_handler,
    };
    static const httpd_uri_t icon_uri = {
        .uri = "/icon.svg", .method = HTTP_GET, .handler = icon_get_handler,
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
    static const httpd_uri_t track_uri = {
        .uri = "/api/track", .method = HTTP_GET, .handler = track_get_handler,
    };
    static const httpd_uri_t events_uri = {
        .uri = "/api/events", .method = HTTP_GET, .handler = events_get_handler,
    };
    static const httpd_uri_t wifi_uri = {
        .uri = "/api/wifi", .method = HTTP_POST, .handler = wifi_post_handler,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &index_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &style_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &appjs_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &manifest_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &swjs_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &icon_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &status_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &gnss_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &device_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &track_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &events_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &wifi_uri));

    ESP_LOGI(TAG, "http server on port %d (web + rest + sse)", WEB_SERVER_PORT);
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
