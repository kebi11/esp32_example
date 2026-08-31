# ESP32-S3 GNSS 网络定位终端

基于 ESP-IDF 5.x 的工程骨架。核心链路：

```text
GNSS → UART → ESP32-S3 → Wi-Fi → HTTP → Browser
```

## 当前状态

**骨架阶段（Skeleton）** —— 目录结构、CMake 组织、Kconfig 配置项、模块 API 边界已就位，
各模块内部实现均为占位（stub），日志会输出 `not ready`。

| Phase | 内容 | 状态 |
|---|---|---|
| 0 | 工程骨架 / Hello World 日志 | 骨架已完成，待编译验证 |
| 1 | GNSS UART 原始数据 | 未实现 |
| 2 | NMEA Parser（GGA / RMC） | 未实现 |
| 3 | Wi-Fi STA | 未实现 |
| 4 | HTTP Server | 未实现 |
| 5 | REST API `/api/status`、`/api/gnss` | 未实现 |
| 6 | Web Dashboard | 前端文件已写，待接入 |
| 7 | NVS 存储 Wi-Fi 凭据 | 未实现 |
| 8 | AP 配网 | 未实现 |
| 9 | 网络可靠性 / 看门狗 | 未实现 |

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
| `GNSS_UART_BAUDRATE` | 9600 | 波特率 |
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
