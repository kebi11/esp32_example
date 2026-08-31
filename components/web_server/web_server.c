#include "web_server.h"

#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "WEB";

/* TODO(Phase 4): httpd_start(&server, &config)，server.uri_match_fn = httpd_uri_match_wildcard
 * TODO(Phase 5): 注册 handler
 *   httpd_register_uri_handler(server, &(httpd_uri_t){
 *       .uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler })
 * TODO(Phase 6): 用 extern const uint8_t index_html_start[] 等符号提供 web/ 下的静态文件
 *   httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start)
 * TODO(Phase 5): JSON 用 cJSON 组装，记得 cJSON_Delete 释放
 */

esp_err_t web_server_start(void)
{
    // TODO(Phase 4): 启动服务器，端口 WEB_SERVER_PORT
    ESP_LOGW(TAG, "web_server_start not implemented yet (port %d)", WEB_SERVER_PORT);
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t web_server_stop(void)
{
    // TODO(Phase 4): httpd_stop(server)
    ESP_LOGW(TAG, "web_server_stop not implemented yet");
    return ESP_ERR_NOT_SUPPORTED;
}
