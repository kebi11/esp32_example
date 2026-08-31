# REST API

所有响应 `Content-Type: application/json; charset=utf-8`，均由 `cJSON` 组装。

## GET /api/status

设备与网络状态。

```json
{
  "device": "esp32s3-gnss-01",
  "uptime": 3251,
  "free_heap": 238944,
  "wifi_connected": true,
  "wifi_rssi": -51,
  "ip": "192.168.1.105"
}
```

| 字段 | 类型 | 来源 | 说明 |
|---|---|---|---|
| `device` | string | `APP_DEVICE_NAME` | 设备名 |
| `uptime` | int | `system_monitor` | 上电秒数 |
| `free_heap` | int | `system_monitor` | 剩余堆字节数 |
| `wifi_connected` | bool | `wifi_manager` | 是否已连上 AP |
| `wifi_rssi` | int | `wifi_manager` | 信号强度 dBm |
| `ip` | string | `wifi_manager` | IPv4 地址 |

## GET /api/gnss

定位信息。

```json
{
  "fix": true,
  "latitude": 31.2304,
  "longitude": 121.4737,
  "altitude": 12.6,
  "speed": 0.3,
  "satellites": 14,
  "utc": "2026-09-01T01:10:30Z"
}
```

| 字段 | 类型 | 来源 | 说明 |
|---|---|---|---|
| `fix` | bool | `gnss_data_t.fix_valid` | 定位是否有效 |
| `latitude` | double | GGA / RMC | 十进制度，北纬为正 |
| `longitude` | double | GGA / RMC | 十进制度，东经为正 |
| `altitude` | double | GGA | 海拔，米 |
| `speed` | double | RMC | 地面速度，km/h（节 × 1.852） |
| `satellites` | int | GGA | 参与定位卫星数 |
| `utc` | string | RMC 日期 + 时间 | ISO-8601，UTC |

未定位时 `fix` 为 `false`，数值字段仍返回最近一次解析结果（或 0）。

## GET /api/device

整合视图，Phase 5 之后提供。

```json
{
  "system": {},
  "wifi": {},
  "gnss": {}
}
```

## GET /

返回 Web Dashboard（`web/index.html`），Phase 6 接入。

## 测试

```bash
curl http://<esp32-ip>/api/status
curl http://<esp32-ip>/api/gnss
```
