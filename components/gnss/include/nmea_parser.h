/**
 * @file nmea_parser.h
 * @brief NMEA-0183 解析器。
 *
 * 第一版只处理 GGA 与 RMC：
 *   - GGA：经纬度、定位质量、卫星数量、海拔
 *   - RMC：UTC 时间、日期、经纬度、地面速度、定位有效状态
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "gnss.h"

#ifdef __cplusplus
extern "C" {
#endif

/** NMEA 单行最大长度（含 $、*XX、CRLF） */
#define NMEA_LINE_MAX_LEN 128

/**
 * @brief 校验 NMEA 语句的校验和（* 号后两位十六进制）。
 *
 * @param sentence 以 '$' 开头的完整语句。
 * @return true 校验通过。
 */
bool nmea_verify_checksum(const char *sentence);

/**
 * @brief 解析单条 NMEA 语句，把结果合并进 @p out。
 *
 * 只更新该语句包含的字段，其他字段保持不变。
 *
 * @param sentence 以 '$' 开头的完整语句。
 * @param[out] out 定位快照，调用方负责清零。
 * @return ESP_OK 已识别并解析；
 *         ESP_ERR_NOT_FOUND 不是本版支持的语句；
 *         ESP_ERR_INVALID_CRC 校验和错误；
 *         ESP_ERR_INVALID_ARG 参数为空或格式非法。
 */
esp_err_t nmea_parse_line(const char *sentence, gnss_data_t *out);

#ifdef __cplusplus
}
#endif
