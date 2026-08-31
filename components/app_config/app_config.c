#include "app_config.h"

#include "esp_log.h"

static const char *TAG = "APP_CONFIG";

esp_err_t app_config_init(void)
{
    ESP_LOGI(TAG, "device      : %s", APP_DEVICE_NAME);
    ESP_LOGI(TAG, "wifi ssid   : %s", APP_WIFI_SSID[0] ? APP_WIFI_SSID : "(not set)");
    ESP_LOGI(TAG, "wifi retry  : %d", WIFI_RETRY_MAX);
    ESP_LOGI(TAG, "http port   : %d", WEB_SERVER_PORT);
    ESP_LOGI(TAG, "gnss uart   : UART%d @ %d baud", GNSS_UART_NUM, GNSS_UART_BAUDRATE);
    ESP_LOGI(TAG, "gnss rx/tx  : GPIO%d / GPIO%d", GNSS_UART_RX_GPIO, GNSS_UART_TX_GPIO);

    return ESP_OK;
}
