#include "nmea_parser.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "NMEA";

bool nmea_verify_checksum(const char *sentence)
{
    // TODO(Phase 2): 计算 '$' 与 '*' 之间的 XOR，与 '*' 后两位十六进制比较
    (void)sentence;
    return false;
}

esp_err_t nmea_parse_line(const char *sentence, gnss_data_t *out)
{
    // TODO(Phase 2): 实现 GGA
    //   $xxGGA,time,lat,N/S,lon,E/W,quality,sats,hdop,alt,M,...*CS
    // TODO(Phase 2): 实现 RMC
    //   $xxRMC,time,status,lat,N/S,lon,E/W,speed,course,date,...*CS
    // 注意：ddmm.mmmm -> 十进制度；speed 节 -> km/h（*1.852）
    if (sentence == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "parser not implemented, dropping: %s", sentence);
    return ESP_ERR_NOT_SUPPORTED;
}
