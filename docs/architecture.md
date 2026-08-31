# 架构说明

## 核心链路

```text
GNSS → UART → ESP32-S3 → Wi-Fi → HTTP → Browser
```

只要这条链路稳定，地图、VPS、MQTT、OTA、数据库都只是扩展。

## 分层

```text
┌─────────────────────────────────────┐
│              Browser                │
│         HTML / CSS / JS             │
└─────────────────┬───────────────────┘
                  │ HTTP
                  ▼
┌─────────────────────────────────────┐
│            Web Server               │
│         esp_http_server             │
│  /api/status      /api/gnss         │
└──────────┬─────────────────┬────────┘
           │                 │
           ▼                 ▼
┌─────────────────────┐ ┌─────────────────────┐
│   System Monitor    │ │      GNSS Core      │
│ uptime / heap       │ │ gnss_data_t 快照    │
└──────────┬──────────┘ └──────────┬──────────┘
           │                       ▼
┌─────────────────────┐ ┌─────────────────────┐
│    Wi-Fi Manager    │ │    NMEA Parser      │
│ STA / Reconnect     │ │    GGA / RMC        │
└──────────┬──────────┘ └──────────┬──────────┘
           │                       ▼
           │              ┌─────────────────────┐
           │              │    UART Driver      │
           │              └──────────┬──────────┘
           ▼                         ▼
      Wi-Fi Router              GNSS Module
```

## 模块职责

| 组件 | 职责 | 对外接口 |
|---|---|---|
| `app_config` | Kconfig → 宏映射，打印生效配置 | `app_config_init()` |
| `storage` | NVS 读写（namespace `app_config`） | `storage_init()`、`storage_get_wifi_credentials()`、`storage_set_wifi_credentials()` |
| `gnss` | UART 接收、NMEA 解析、维护定位快照 | `gnss_init()`、`gnss_start()`、`gnss_get_latest()` |
| `wifi_manager` | STA 连接、断线重连、状态查询 | `wifi_manager_init()`、`wifi_manager_start()`、`wifi_manager_get_status()` |
| `web_server` | HTTP Server 与 REST API | `web_server_start()`、`web_server_stop()` |
| `system_monitor` | uptime / heap 采样 | `system_monitor_start()`、`system_monitor_get_status()` |
| `main` | 仅负责启动顺序 | `app_main()` |

## 模块边界

禁止跨模块直接访问内部变量。

```text
错误：web_server.c 直接读 gnss.c 的全局变量
正确：gnss_data_t data; gnss_get_latest(&data);
```

## 并发

`gnss_task` 写快照，`web_server` 读快照：

```text
gnss_task --write--> gnss_data_t <--read-- web_server
```

第一版用 FreeRTOS Mutex 保护共享结构体，后续可换 Queue。

## 错误处理

- 初始化阶段可用 `ESP_ERROR_CHECK`
- 运行阶段不因临时错误让整机崩溃：Wi-Fi 掉线 → 记录日志 → 等待 → 重连，而不是重启
- 骨架阶段各模块未实现时返回 `ESP_ERR_NOT_SUPPORTED`，`app_main` 只打 warning

## 依赖方向

```text
main
 ├── app_config ── (无)
 ├── storage ── nvs_flash
 ├── gnss ── app_config, driver
 ├── wifi_manager ── app_config, storage, esp_wifi, esp_netif
 ├── web_server ── app_config, gnss, wifi_manager, system_monitor, esp_http_server, json
 └── system_monitor ── esp_timer
```
