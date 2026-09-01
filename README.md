# ESP32-S3 GNSS 网络定位终端

基于 ESP-IDF 5.x 的工程骨架。核心链路：

```text
GNSS → UART → ESP32-S3 → Wi-Fi → HTTP → Browser
```

## 当前状态

目录结构、CMake 组织、Kconfig 配置项、模块 API 边界已就位。
GNSS（Phase 1+2）与 Wi-Fi STA（Phase 3）已实现，其余模块仍是占位 stub。

| Phase | 内容 | 状态 |
|---|---|---|
| 0 | 工程骨架 / Hello World 日志 | 已完成 |
| 1 | GNSS UART 原始数据 | 已完成（实测 115200 波特率，串口输出 `$GNRMC`） |
| 2 | NMEA Parser（GGA / RMC） | 已实现（RMC 解析 + mutex 快照 + 每秒摘要日志），待上板验证 |
| 3 | Wi-Fi STA | 已完成（实测连上手机热点，RSSI -17，IP 192.168.168.105） |
| 4 | HTTP Server | 已实现（esp_http_server，端口 80），待上板验证 |
| 5 | REST API `/api/status`、`/api/gnss` | 已实现（含 `/api/device` 汇总），待上板验证 |
| 6 | Web Dashboard | 已实现（前端三件套嵌入固件 + `/` 根路由），待上板验证 |
| 7 | NVS 存储 Wi-Fi 凭据 | 已实现（因 Wi-Fi 依赖 NVS 提前完成） |
| 8 | AP 配网 | 已实现（无凭据/重试超限回退 AP，网页提交凭据） |
| 9 | 网络可靠性 / 看门狗 | 已实现（UART 异常恢复 + task WDT + 系统监控任务） |
| 10 | 扩展 | 部分：SSE 实时推送、地图、轨迹存储、PWA 已实现（MQTT/OTA/HTTPS/microSD 需外部资源，未做） |

## 目录结构

```text
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   └── app_main.c              只负责启动顺序
├── components/
│   ├── app_config/             项目级配置宏（Kconfig 映射）
│   ├── wifi_manager/           STA 连接与重连
│   ├── gnss/                   UART 接收 + NMEA 解析
│   ├── web_server/             esp_http_server + REST API
│   ├── storage/                NVS 读写
│   └── system_monitor/         uptime / heap / RSSI 统计
├── web/                        前端静态文件（Phase 6 嵌入）
└── docs/                       architecture / api / hardware / development
```

## 配置

```bash
idf.py set-target esp32s3
idf.py menuconfig
```

主要配置项：

| 配置 | 默认 | 说明 |
|---|---|---|
| `APP_DEVICE_NAME` | `esp32s3-gnss-01` | 设备名 |
| `APP_WIFI_SSID` / `APP_WIFI_PASSWORD` | 空 | 编译期默认 Wi-Fi 凭据，运行期以 NVS 优先 |
| `APP_WIFI_RETRY_MAX` | 10 | STA 最大重试次数 |
| `APP_WEB_SERVER_PORT` | 80 | HTTP 端口 |
| `GNSS_UART_NUM` | 1 | UART 端口号 |
| `GNSS_UART_BAUDRATE` | 115200 | 波特率（本模块实测值，见 docs/hardware.md） |
| `GNSS_UART_RX_GPIO` | 18 | ESP32 RX ← GNSS TXD |
| `GNSS_UART_TX_GPIO` | 17 | ESP32 TX → GNSS RXD |

## 构建

```bash
idf.py build
idf.py -p COMx flash monitor
```

退出 Monitor：`Ctrl + ]`

## 文档

- [架构说明](docs/architecture.md)
- [REST API](docs/api.md)
- [硬件接线](docs/hardware.md)
- [开发流程](docs/development.md)
