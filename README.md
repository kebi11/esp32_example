# ESP32-S3 GNSS Network Positioning Terminal

ESP-IDF 5.x firmware that reads a GNSS module over UART, connects over Wi-Fi, and serves a live location dashboard in the browser.

[Chinese README](README.zh.md)

```text
GNSS → UART → ESP32-S3 → Wi-Fi → HTTP → Browser
```

## Status

Project layout, CMake, Kconfig, and module APIs are in place.
GNSS (Phase 1–2) and Wi-Fi STA (Phase 3) are implemented. Later phases are implemented in firmware but still need more on-device verification where noted.

| Phase | Scope | Status |
|---|---|---|
| 0 | Project skeleton / Hello World logs | Done |
| 1 | GNSS UART raw data | Done (tested at 115200 baud, serial shows `$GNRMC`) |
| 2 | NMEA parser (GGA / RMC) | Implemented (RMC parse + mutex snapshot + 1 Hz summary log), board verification pending |
| 3 | Wi-Fi STA | Done (tested on phone hotspot, RSSI -17, got an IP) |
| 4 | HTTP server | Implemented (`esp_http_server`, port 80), board verification pending |
| 5 | REST API `/api/status`, `/api/gnss` | Implemented (plus `/api/device` summary), board verification pending |
| 6 | Web dashboard | Implemented (frontend embedded in firmware, `/` route), board verification pending |
| 7 | NVS Wi-Fi credentials | Implemented (done early because Wi-Fi depends on NVS) |
| 8 | AP provisioning | Implemented (falls back to AP if no credentials / retry limit; submit SSID/password from the page) |
| 9 | Reliability / watchdog | Implemented (UART recovery + task WDT + system monitor task) |
| 10 | Extras | Partial: SSE live push, map, track storage, PWA done. MQTT / OTA / HTTPS / microSD need extra hardware or infra |

## Layout

```text
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   └── app_main.c              boot order only
├── components/
│   ├── app_config/             project Kconfig macros
│   ├── wifi_manager/           STA connect + reconnect
│   ├── gnss/                   UART RX + NMEA parse
│   ├── web_server/             esp_http_server + REST API
│   ├── storage/                NVS read/write
│   └── system_monitor/         uptime / heap / RSSI
├── web/                        frontend (embedded in Phase 6)
└── docs/                       architecture / api / hardware / development
```

## Configure

```bash
idf.py set-target esp32s3
idf.py menuconfig
```

| Option | Default | Notes |
|---|---|---|
| `APP_DEVICE_NAME` | `esp32s3-gnss-01` | Device name |
| `APP_WIFI_SSID` / `APP_WIFI_PASSWORD` | empty | Compile-time defaults; NVS wins at runtime |
| `APP_WIFI_RETRY_MAX` | 10 | STA retry limit |
| `APP_WEB_SERVER_PORT` | 80 | HTTP port |
| `GNSS_UART_NUM` | 1 | UART port |
| `GNSS_UART_BAUDRATE` | 115200 | Measured on this module; see `docs/hardware.md` |
| `GNSS_UART_RX_GPIO` | 18 | ESP32 RX ← GNSS TXD |
| `GNSS_UART_TX_GPIO` | 17 | ESP32 TX → GNSS RXD |

## Build

```bash
idf.py build
idf.py -p COMx flash monitor
```

Leave the monitor with `Ctrl + ]`.

## Docs

- [Architecture](docs/architecture.md)
- [REST API](docs/api.md)
- [Hardware wiring](docs/hardware.md)
- [Development notes](docs/development.md)
- [Chinese README](README.zh.md)
