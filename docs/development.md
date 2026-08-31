# 开发流程

## 日常循环

```text
打开项目 → 选择 ESP32-S3 Target → 改代码 → Build → Flash → Monitor → 测试 → Git Commit
```

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

退出 Monitor：`Ctrl + ]`。也可用 VS Code 底部按钮完成。

## 阶段划分

| Phase | 内容 | 验收 |
|---|---|---|
| 0 | 工程骨架 / Hello World 日志 | 串口看到启动日志 |
| 1 | GNSS UART 原始数据 | 串口打印 `$GNGGA` / `$GNRMC`，不解析 |
| 2 | NMEA Parser（GGA / RMC） | 输出 Fix / Satellites / Lat / Lon / Alt / Speed |
| 3 | Wi-Fi STA | 打印 `Wi-Fi connected` + IP + RSSI |
| 4 | HTTP Server | 浏览器打开 `http://<ip>/` 返回页面 |
| 5 | REST API | curl 可请求 `/api/status`、`/api/gnss` |
| 6 | Web Dashboard | 页面每秒刷新 |
| 7 | NVS | Wi-Fi 凭据断电保留 |
| 8 | AP 配网 | 手机连 `ESP32-GNSS-Setup` 配置 Wi-Fi |
| 9 | 网络可靠性 | 自动重连、UART 恢复、看门狗 |
| 10 | 扩展 | SSE / WebSocket / MQTT / OTA / 地图 |

严格按顺序推进，不要反过来先写前端。

## 分支

```text
main
├── feature/gnss-uart
├── feature/nmea-parser
├── feature/wifi-sta
├── feature/web-server
└── feature/nvs-config
```

## 代码风格

- 公开函数：`gnss_init()` / `gnss_get_latest()` / `wifi_manager_start()`
- 私有函数：`static void gnss_task(void *arg);`
- 变量 `snake_case`，类型 `gnss_data_t`，宏 `GNSS_UART_NUM`
- 每个模块自己的 `static const char *TAG`

## 日志

统一 `esp_log.h`，不用 `printf`。

```c
ESP_LOGE  Error
ESP_LOGW  Warning
ESP_LOGI  Info
ESP_LOGD  Debug
ESP_LOGV  Verbose
```

## 错误处理

`esp_err_t` 优先检查。初始化阶段可用 `ESP_ERROR_CHECK`；运行阶段的临时错误（如 Wi-Fi 掉线）记录日志后重试，不重启整机。

## 当前骨架说明

各模块 `*_start()` / `*_init()` 目前返回 `ESP_ERR_NOT_SUPPORTED`，`app_main` 通过 `RUN_STEP()` 宏只打 warning 不阻塞。
按 Phase 顺序把 TODO 逐个填掉即可，模块边界与 CMake 组织无需再改。
