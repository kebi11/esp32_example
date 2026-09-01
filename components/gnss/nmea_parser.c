/**
 * @file nmea_parser.c
 * @brief NMEA-0183 解析实现：校验和 + GGA + RMC。
 *
 * 说明：本项目使用的 S1216F8-BD 默认只输出 RMC（20 Hz），
 * GGA 解析同样实现，待后续开启 GGA 输出后即可直接工作。
 */

#include "nmea_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** 单条语句最多的逗号分隔字段数（RMC 最多约 13 个，留余量） */
#define NMEA_MAX_FIELDS 16

bool nmea_verify_checksum(const char *sentence)
{
    if (sentence == NULL || sentence[0] != '$') {
        return false;
    }

    /* '$' 与 '*' 之间所有字符按位异或 */
    uint8_t computed = 0;
    const char *p = sentence + 1;
    while (*p != '\0' && *p != '*') {
        computed ^= (uint8_t)*p++;
    }
    if (*p != '*') {
        return false; /* 语句没有校验和字段 */
    }

    char *end = NULL;
    long given = strtol(p + 1, &end, 16);
    if (end == p + 1) {
        return false; /* '*' 后不是合法的十六进制 */
    }

    return (uint8_t)given == computed;
}

/**
 * @brief 把语句按逗号切成字段，'*' 及其后的校验和截断。
 *
 * 切分后 fields[0] 是 "$xxRMC" 这样的语句头（talker ID + 类型码）。
 * 空字段（连续逗号之间）会得到空字符串指针，不跳过。
 *
 * @return 字段个数。
 */
static int nmea_split_fields(char *line, char *fields[], int max_fields)
{
    int count = 1;
    fields[0] = line;

    for (char *p = line; *p != '\0' && count < max_fields; p++) {
        if (*p == ',') {
            *p = '\0';
            fields[count++] = p + 1;
        } else if (*p == '*') {
            *p = '\0';
            break;
        }
    }
    return count;
}

/**
 * @brief 判断语句头是否为指定类型（忽略 talker ID，$GN/$GP/$BD 均可）。
 *
 * 语句头形如 "$GNRMC"，跳过 '$' + 2 位 talker ID 后比较类型码。
 */
static bool nmea_is_type(const char *header, const char *type)
{
    size_t type_len = strlen(type);
    if (strlen(header) != 3 + type_len) {
        return false;
    }
    return strcmp(header + 3, type) == 0;
}

/**
 * @brief 解析 ddmm.mmmm / dddmm.mmmm 坐标为十进制度。
 *
 * 纬度 2 位度、经度 3 位度，统一按"整数部分去掉最后两位分钟"处理。
 * 半球为 S/W 时取负。
 *
 * @return true 解析成功。
 */
static bool nmea_parse_coord(const char *value, char hemi, double *out)
{
    if (value == NULL || value[0] == '\0') {
        return false;
    }

    char *end = NULL;
    double raw = strtod(value, &end);
    if (end == value || raw < 0.0) {
        return false;
    }

    int degrees = (int)(raw / 100.0);
    double minutes = raw - degrees * 100.0;
    double result = degrees + minutes / 60.0;

    if (hemi == 'S' || hemi == 'W') {
        result = -result;
    }
    *out = result;
    return true;
}

/** @brief 解析 hhmmss.sss 为时/分/秒。 */
static bool nmea_parse_time(const char *value, gnss_data_t *out)
{
    int hour = 0, minute = 0, second = 0;
    if (value == NULL || sscanf(value, "%2d%2d%2d", &hour, &minute, &second) != 3) {
        return false;
    }
    out->hour = hour;
    out->minute = minute;
    out->second = second;
    return true;
}

/** @brief 解析 ddmmyy 为年/月/日。RMC 只有两位年份，按 2000+yy 处理。 */
static bool nmea_parse_date(const char *value, gnss_data_t *out)
{
    int day = 0, month = 0, yy = 0;
    if (value == NULL || sscanf(value, "%2d%2d%2d", &day, &month, &yy) != 3) {
        return false;
    }
    out->day = day;
    out->month = month;
    out->year = yy + 2000;
    return true;
}

/**
 * @brief 解析 RMC：时间、日期、经纬度、速度、定位状态。
 *
 * 字段：0 语句头 1 时间 2 状态A/V 3 纬度 4 N/S 5 经度 6 E/W
 *       7 速度(节) 8 航向 9 日期 10 磁偏角 11 E/W 12 模式
 */
static esp_err_t nmea_parse_rmc(char *fields[], int count, gnss_data_t *out)
{
    if (count < 10) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 未定位(V)时时间日期仍然有效，经纬度/速度字段通常为空 */
    nmea_parse_time(fields[1], out);
    nmea_parse_date(fields[9], out);

    if (fields[2][0] != 'A') {
        out->fix_valid = false;
        return ESP_OK;
    }

    if (!nmea_parse_coord(fields[3], fields[4][0], &out->latitude) ||
        !nmea_parse_coord(fields[5], fields[6][0], &out->longitude)) {
        out->fix_valid = false;
        return ESP_ERR_INVALID_ARG;
    }

    out->speed_kmh = (float)(strtod(fields[7], NULL) * 1.852); /* 节 -> km/h */
    out->fix_valid = true;
    return ESP_OK;
}

/**
 * @brief 解析 GGA：时间、经纬度、卫星数、海拔、定位质量。
 *
 * 字段：0 语句头 1 时间 2 纬度 3 N/S 4 经度 5 E/W 6 质量因子
 *       7 卫星数 8 HDOP 9 海拔 10 'M' ...
 */
static esp_err_t nmea_parse_gga(char *fields[], int count, gnss_data_t *out)
{
    if (count < 10) {
        return ESP_ERR_INVALID_ARG;
    }

    nmea_parse_time(fields[1], out);

    int quality = atoi(fields[6]);
    out->satellites = atoi(fields[7]);

    if (quality <= 0) {
        out->fix_valid = false;
        return ESP_OK;
    }

    if (!nmea_parse_coord(fields[2], fields[3][0], &out->latitude) ||
        !nmea_parse_coord(fields[4], fields[5][0], &out->longitude)) {
        out->fix_valid = false;
        return ESP_ERR_INVALID_ARG;
    }

    out->altitude_m = (float)strtod(fields[9], NULL);
    out->fix_valid = true;
    return ESP_OK;
}

esp_err_t nmea_parse_line(const char *sentence, gnss_data_t *out)
{
    if (sentence == NULL || out == NULL || sentence[0] != '$') {
        return ESP_ERR_INVALID_ARG;
    }

    if (!nmea_verify_checksum(sentence)) {
        return ESP_ERR_INVALID_CRC;
    }

    /* 切分需要改写内容，先复制到栈缓冲 */
    char buf[NMEA_LINE_MAX_LEN];
    if (strlen(sentence) >= sizeof(buf)) {
        return ESP_ERR_INVALID_ARG;
    }
    strcpy(buf, sentence);

    char *fields[NMEA_MAX_FIELDS];
    int count = nmea_split_fields(buf, fields, NMEA_MAX_FIELDS);

    if (nmea_is_type(fields[0], "RMC")) {
        return nmea_parse_rmc(fields, count, out);
    }
    if (nmea_is_type(fields[0], "GGA")) {
        return nmea_parse_gga(fields, count, out);
    }

    /* 本版不处理的语句类型（GSA/GSV/VTG...），交给调用方静默忽略 */
    return ESP_ERR_NOT_FOUND;
}
