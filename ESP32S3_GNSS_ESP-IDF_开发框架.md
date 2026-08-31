# ESP32-S3 GNSS 网络定位终端
## 基于 VS Code + ESP-IDF Extension 的项目框架搭建与开发说明

> 目标：使用 **ESP32-S3 + SkyTraq S1216F8-BD GNSS 模块 + Wi-Fi**，完成一个可通过浏览器访问的网络定位终端。  
> 开发环境以 **VS Code + Espressif IDF Extension + ESP-IDF 5.x** 为核心，不使用 Arduino Framework。

---

# 1. 项目目标

第一阶段实现以下核心能力：

1. ESP32-S3 正常启动并输出日志
2. 通过 UART 接收 GNSS 模块的 NMEA 数据
3. 解析基本定位信息
   - UTC 时间
   - 纬度
   - 经度
   - 海拔
   - 速度
   - 卫星数量
   - 定位状态
4. ESP32-S3 连接普通 2.4 GHz Wi-Fi
5. 使用 NVS 保存 Wi-Fi 配置
6. ESP32-S3 启动 HTTP Server
7. 提供 REST API
8. 浏览器查看设备状态和定位信息
9. 后续扩展 AP 配网、SSE/WebSocket、VPS、MQTT、OTA

---

# 2. 推荐技术栈

## 2.1 开发环境

```text
Windows
└── Visual Studio Code
    └── Espressif IDF Extension
        └── ESP-IDF 5.x
            ├── Python
            ├── CMake
            ├── Ninja
            ├── Xtensa Toolchain
            ├── esptool.py
            └── OpenOCD
```

推荐：

- 编辑器：VS Code
- 插件：Espressif IDF
- 开发框架：ESP-IDF 5.x
- 语言：C 为主，必要时使用 C++
- 构建系统：CMake + Ninja
- RTOS：FreeRTOS
- 版本控制：Git

---

# 3. VS Code 环境搭建

## 3.1 安装 VS Code

安装 Visual Studio Code。

建议安装后先确认命令行可用：

```powershell
code --version
```

---

## 3.2 安装 ESP-IDF 插件

在 VS Code 扩展市场搜索：

```text
Espressif IDF
```

安装官方：

```text
Espressif IDF
Publisher: Espressif Systems
```

---

## 3.3 配置 ESP-IDF

打开命令面板：

```text
Ctrl + Shift + P
```

执行：

```text
ESP-IDF: Configure ESP-IDF Extension
```

首次安装建议选择：

```text
Express
```

让插件自动安装和配置：

- ESP-IDF
- Python 虚拟环境
- CMake
- Ninja
- 编译工具链
- OpenOCD
- esptool

---

## 3.4 检查 ESP-IDF 环境

打开 VS Code 命令面板：

```text
ESP-IDF: Open ESP-IDF Terminal
```

执行：

```bash
idf.py --version
```

正常应看到类似：

```text
ESP-IDF v5.x.x
```

---

# 4. 创建工程

推荐工程名：

```text
esp32s3-gnss-terminal
```

在 VS Code 中执行：

```text
ESP-IDF: New Project
```

目标芯片：

```text
ESP32-S3
```

建议先基于：

```text
hello_world
```

或空白模板创建。

---

# 5. 项目目录规划

推荐不要把所有代码全部堆在 `main.c`。

建议从一开始就按照模块化方式组织：

```text
esp32s3-gnss-terminal/
│
├── CMakeLists.txt
├── sdkconfig
├── sdkconfig.defaults
├── partitions.csv
├── README.md
├── .gitignore
│
├── main/
│   ├── CMakeLists.txt
│   └── app_main.c
│
├── components/
│   │
│   ├── app_config/
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── app_config.h
│   │   └── app_config.c
│   │
│   ├── wifi_manager/
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── wifi_manager.h
│   │   └── wifi_manager.c
│   │
│   ├── gnss/
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   ├── gnss.h
│   │   │   └── nmea_parser.h
│   │   ├── gnss.c
│   │   └── nmea_parser.c
│   │
│   ├── web_server/
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── web_server.h
│   │   └── web_server.c
│   │
│   ├── storage/
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── storage.h
│   │   └── storage.c
│   │
│   └── system_monitor/
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── system_monitor.h
│       └── system_monitor.c
│
├── web/
│   ├── index.html
│   ├── style.css
│   └── app.js
│
└── docs/
    ├── architecture.md
    ├── api.md
    ├── hardware.md
    └── development.md
```

---

# 6. 各模块职责

## 6.1 `main/app_main.c`

只负责系统启动顺序。

不要在这里写大量业务逻辑。

推荐：

```text
app_main()
│
├── 初始化 NVS
├── 初始化配置
├── 初始化 GNSS
├── 启动 Wi-Fi
├── 启动 Web Server
└── 启动系统监控
```

示意：

```c
void app_main(void)
{
    storage_init();
    app_config_init();

    gnss_init();
    wifi_manager_init();
    web_server_start();
    system_monitor_start();
}
```

---

# 7. GNSS 模块设计

硬件：

```text
SkyTraq S1216F8-BD
        │
        │ UART
        ▼
     ESP32-S3
```

建议第一阶段只接：

```text
GNSS VCC  -> ESP32 3.3V
GNSS GND  -> ESP32 GND
GNSS TXD  -> ESP32 RX
```

需要配置 GNSS 时再接：

```text
GNSS RXD <- ESP32 TX
```

PPS 暂时保留。

---

## 7.1 GNSS 数据结构

建议统一保存为：

```c
typedef struct {
    bool fix_valid;

    double latitude;
    double longitude;

    float altitude_m;
    float speed_kmh;

    int satellites;

    int year;
    int month;
    int day;

    int hour;
    int minute;
    int second;
} gnss_data_t;
```

---

## 7.2 NMEA 第一阶段只处理

优先：

```text
GGA
RMC
```

### GGA

主要获得：

- 经纬度
- 定位质量
- 卫星数量
- 海拔

### RMC

主要获得：

- UTC 时间
- 日期
- 经纬度
- 地面速度
- 定位有效状态

不要第一版就解析所有 NMEA 语句。

---

# 8. FreeRTOS 任务规划

推荐初期任务：

```text
FreeRTOS
│
├── gnss_task
│   └── UART 接收并解析 NMEA
│
├── wifi_manager
│   └── Wi-Fi 连接和重连
│
├── http_server
│   └── ESP-IDF HTTP Server
│
└── system_monitor_task
    └── 运行状态统计
```

---

## 8.1 GNSS Task

职责：

```text
UART 接收
   ↓
按行提取 NMEA
   ↓
校验
   ↓
解析
   ↓
更新 gnss_data_t
```

不要在 UART 接收函数里直接处理 Web 或 Wi-Fi。

---

# 9. Wi-Fi 模块规划

第一版只实现：

```text
Wi-Fi STA
```

即：

```text
ESP32-S3
    │
    │ SSID + Password
    ▼
家庭/宿舍路由器
```

---

## 9.1 后续增加 AP 配网

第二阶段：

```text
开机
 │
 ▼
读取 NVS
 │
 ├── 有 Wi-Fi 配置
 │       │
 │       ▼
 │    STA 连接
 │
 └── 无配置/连接失败
         │
         ▼
     开启 AP
         │
         ▼
   ESP32-GNSS-Setup
         │
         ▼
     手机配置页面
```

---

# 10. NVS 配置规划

建议保存：

```text
namespace: app_config

wifi_ssid
wifi_password
device_name
server_url
mqtt_host
```

第一版只需要：

```text
wifi_ssid
wifi_password
```

不要将 Wi-Fi 密码直接长期写死在 Git 仓库源码中。

---

# 11. Web Server

使用 ESP-IDF 官方组件：

```text
esp_http_server
```

ESP32 本身作为 HTTP Server。

---

## 11.1 第一版 API

### 获取设备状态

```http
GET /api/status
```

返回：

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

---

### 获取 GNSS 信息

```http
GET /api/gnss
```

返回：

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

---

### 获取完整设备信息

```http
GET /api/device
```

后期可以整合：

```json
{
  "system": {},
  "wifi": {},
  "gnss": {}
}
```

---

# 12. Web 前端

第一阶段使用：

```text
HTML
CSS
JavaScript
```

不使用 Vue。

原因：

- 文件体积更小
- 开发简单
- 无额外 Node.js 构建流程
- 更适合先理解 ESP32 HTTP Server

---

## 12.1 页面内容

首页：

```text
ESP32-S3 GNSS Terminal
─────────────────────

设备状态
IP              192.168.1.105
Wi-Fi RSSI      -51 dBm
运行时间        00:54:11
剩余内存        238 KB

GNSS
定位状态        已定位
卫星数量        14
纬度            31.230400
经度            121.473700
海拔            12.6 m
速度            0.3 km/h
UTC             2026-09-01 01:10:30
```

---

## 12.2 数据刷新

第一阶段：

```text
HTTP Polling
```

浏览器每 1 秒：

```http
GET /api/status
GET /api/gnss
```

第二阶段再考虑：

```text
SSE
```

第三阶段再考虑：

```text
WebSocket
```

---

# 13. CMake 组织原则

根目录：

```cmake
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp32s3_gnss_terminal)
```

`main/CMakeLists.txt`：

```cmake
idf_component_register(
    SRCS
        "app_main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        app_config
        wifi_manager
        gnss
        web_server
        storage
        system_monitor
)
```

每个模块单独作为 ESP-IDF Component。

---

# 14. 日志规范

统一使用 ESP-IDF Logger：

```c
#include "esp_log.h"
```

例如：

```c
static const char *TAG = "GNSS";

ESP_LOGI(TAG, "GNSS initialized");
ESP_LOGW(TAG, "No valid GNSS fix");
ESP_LOGE(TAG, "UART initialization failed");
```

不要大量使用：

```c
printf()
```

推荐日志等级：

```text
ESP_LOGE  Error
ESP_LOGW  Warning
ESP_LOGI  Info
ESP_LOGD  Debug
ESP_LOGV  Verbose
```

---

# 15. 错误处理原则

ESP-IDF API 返回 `esp_err_t` 时优先检查。

初始化阶段可使用：

```c
ESP_ERROR_CHECK(...)
```

普通运行阶段不要因为一个临时错误直接让整机崩溃。

例如 Wi-Fi 掉线：

```text
检测断线
  ↓
记录日志
  ↓
等待
  ↓
重新连接
```

而不是：

```text
Wi-Fi 断开
  ↓
ESP32 重启
```

---

# 16. Git 规划

初始化：

```bash
git init
git add .
git commit -m "chore: initialize ESP32-S3 GNSS project"
```

建议分支：

```text
main
│
├── feature/gnss-uart
├── feature/nmea-parser
├── feature/wifi-sta
├── feature/web-server
└── feature/nvs-config
```

---

## 16.1 `.gitignore`

至少忽略：

```gitignore
build/
sdkconfig.old
.vscode/settings.json
dependencies.lock
managed_components/
```

是否忽略 `sdkconfig` 根据项目管理策略决定。

推荐：

- `sdkconfig.defaults` 提交
- `sdkconfig` 可提交，也可根据项目需求忽略
- 不提交个人路径和敏感 Wi-Fi 密码

---

# 17. 开发阶段划分

## Phase 0：环境验证

目标：

```text
ESP-IDF 可用
ESP32-S3 可编译
ESP32-S3 可烧录
串口日志正常
```

验收：

```text
Hello world!
```

---

## Phase 1：GNSS 原始串口

目标：

```text
ESP32-S3
    │ UART
    ▼
GNSS
```

验收：

串口终端可以看到：

```text
$GNGGA,...
$GNRMC,...
```

暂时不解析。

---

## Phase 2：NMEA Parser

目标：

解析：

```text
GGA
RMC
```

验收输出：

```text
Fix: YES
Satellites: 12
Lat: xx.xxxxxx
Lon: xxx.xxxxxx
Altitude: xx.x m
Speed: x.x km/h
```

---

## Phase 3：Wi-Fi STA

目标：

ESP32 自动连接路由器。

验收：

```text
Wi-Fi connected
IP: 192.168.x.x
RSSI: -xx dBm
```

---

## Phase 4：HTTP Server

目标：

浏览器访问：

```text
http://ESP32_IP/
```

返回：

```text
ESP32-S3 GNSS Terminal
```

---

## Phase 5：REST API

完成：

```text
GET /api/status
GET /api/gnss
```

浏览器/PowerShell/curl 均可请求。

---

## Phase 6：Web Dashboard

显示：

```text
系统状态
Wi-Fi 状态
GNSS 状态
经纬度
速度
海拔
卫星数量
时间
```

每秒刷新。

---

## Phase 7：NVS

实现：

```text
Wi-Fi SSID
Wi-Fi Password
```

断电后仍保留。

---

## Phase 8：AP 配网

实现：

```text
ESP32-GNSS-Setup
```

手机浏览器输入 Wi-Fi 信息。

---

## Phase 9：网络可靠性

实现：

- Wi-Fi 自动重连
- GNSS UART 异常恢复
- HTTP Server 稳定运行
- Watchdog
- 错误日志

---

## Phase 10：后续扩展

可选：

```text
PPS 精确授时
SSE
WebSocket
MQTT
HTTPS
VPS
轨迹存储
microSD
OTA
地图显示
手机 PWA
```

---

# 18. 第一阶段不要做的事情

暂时不要：

- Vue 3
- React
- MQTT
- 数据库
- VPS
- WebSocket
- OTA
- microSD
- 蓝牙
- GPS 地图
- 复杂 UI

原因：

先完成最小可运行链路：

```text
GNSS
 ↓
ESP32
 ↓
Wi-Fi
 ↓
HTTP API
 ↓
Browser
```

这是项目的核心。

---

# 19. 推荐开发顺序

严格按照：

```text
1. Hello World
        ↓
2. UART
        ↓
3. NMEA 原始数据
        ↓
4. NMEA Parser
        ↓
5. GNSS Data Model
        ↓
6. Wi-Fi STA
        ↓
7. HTTP Server
        ↓
8. REST API
        ↓
9. Web UI
        ↓
10. NVS
        ↓
11. AP 配网
        ↓
12. 网络增强
        ↓
13. 云端扩展
```

不要反过来先写漂亮的前端。

---

# 20. 初始软件架构

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
│                                     │
│ /api/status       /api/gnss         │
└──────────────┬──────────────┬───────┘
               │              │
               ▼              ▼
┌─────────────────────┐ ┌─────────────────────┐
│   System Monitor    │ │      GNSS Core      │
│                     │ │                     │
│ uptime              │ │ latitude            │
│ heap                │ │ longitude           │
│ RSSI                │ │ altitude            │
│ IP                  │ │ speed               │
└──────────┬──────────┘ │ satellites          │
           │            └──────────┬──────────┘
           ▼                       │
┌─────────────────────┐            ▼
│    Wi-Fi Manager    │   ┌───────────────────┐
│                     │   │    NMEA Parser    │
│ STA                 │   │    GGA / RMC      │
│ Reconnect           │   └─────────┬─────────┘
└──────────┬──────────┘             │
           │                        ▼
           │              ┌───────────────────┐
           │              │    UART Driver    │
           │              └─────────┬─────────┘
           │                        │
           ▼                        ▼
      Wi-Fi Router             GNSS Module
```

---

# 21. 模块之间的原则

模块不要互相随意访问内部变量。

错误方式：

```text
web_server.c
直接读取 gnss.c 内部全局变量
```

推荐：

```c
gnss_data_t data;
gnss_get_latest(&data);
```

即：

```text
Web Server
    │
    ▼
gnss_get_latest()
    │
    ▼
GNSS Module
```

保持 API 边界。

---

# 22. 并发数据访问

GNSS Task 不断更新数据，同时 Web Server 会读取数据。

因此后期需要考虑：

```text
gnss_task
    │
    │ write
    ▼
gnss_data
    ▲
    │ read
web_server
```

推荐使用：

```text
FreeRTOS Mutex
```

或者：

```text
Queue
```

第一版可用 Mutex 保护共享结构体。

---

# 23. 建议的代码风格

函数：

```c
gnss_init();
gnss_start();
gnss_get_latest();

wifi_manager_init();
wifi_manager_start();

web_server_start();

storage_init();
```

内部私有函数：

```c
static void gnss_task(void *arg);
static void wifi_event_handler(...);
```

变量：

```c
snake_case
```

类型：

```c
gnss_data_t
wifi_status_t
```

宏：

```c
GNSS_UART_NUM
GNSS_RX_GPIO
WIFI_RETRY_MAX
```

---

# 24. 初始配置建议

统一在：

```text
components/app_config/include/app_config.h
```

保存项目级默认参数。

例如：

```c
#define APP_DEVICE_NAME "esp32s3-gnss"

#define GNSS_UART_BAUDRATE 9600

#define WIFI_RETRY_MAX 10

#define WEB_SERVER_PORT 80
```

GPIO 编号等硬件相关配置在确认开发板型号和实际接线后再填写。

---

# 25. VS Code 日常开发流程

每次开发：

```text
打开项目
  ↓
选择 ESP32-S3 Target
  ↓
修改代码
  ↓
Build
  ↓
Flash
  ↓
Monitor
  ↓
测试
  ↓
Git Commit
```

对应命令：

```bash
idf.py set-target esp32s3
idf.py build
idf.py flash
idf.py monitor
```

也可使用 VS Code ESP-IDF 插件底部按钮完成。

退出 Monitor：

```text
Ctrl + ]
```

---

# 26. 第一次正式开发任务

当前不要直接搭完整系统。

第一个正式任务定义为：

> **建立 ESP-IDF ESP32-S3 工程并完成 Hello World + 串口日志验证。**

成功标准：

- [ ] VS Code ESP-IDF 插件配置完成
- [ ] `idf.py --version` 正常
- [ ] 工程目标为 ESP32-S3
- [ ] `idf.py build` 成功
- [ ] 能识别串口
- [ ] `idf.py flash` 成功
- [ ] `idf.py monitor` 可以看到运行日志
- [ ] ESP32-S3 可以正常复位重新运行

完成之后，再进入：

> **Phase 1：GNSS UART 接入**

---

# 27. 项目最终演进方向

```text
                  ┌───────────────┐
                  │   GNSS/北斗   │
                  └──────┬────────┘
                         │ UART
                         ▼
                  ┌───────────────┐
                  │   ESP32-S3    │
                  │               │
                  │ FreeRTOS      │
                  │ Wi-Fi         │
                  │ NVS           │
                  │ HTTP          │
                  └───┬───────┬───┘
                      │       │
               LAN HTTP       │ MQTT/HTTPS
                      │       │
                      ▼       ▼
                 Browser     VPS
                              │
                       ┌──────┴──────┐
                       │             │
                    Backend       Database
                       │
                       ▼
                    Web App
```

---

# 28. 当前确定的技术决策

| 项目 | 当前选择 |
|---|---|
| MCU | ESP32-S3 |
| GNSS | SkyTraq S1216F8-BD |
| IDE | VS Code |
| VS Code 插件 | Espressif IDF |
| Framework | ESP-IDF 5.x |
| Language | C / C++ |
| RTOS | FreeRTOS |
| GNSS 通信 | UART |
| GNSS 协议 | NMEA |
| 首批 NMEA | GGA + RMC |
| 网络 | Wi-Fi STA |
| 配网 | 后续 AP Provisioning |
| 配置存储 | NVS |
| Web Server | esp_http_server |
| API | REST |
| JSON | cJSON |
| Web 前端 V1 | HTML + CSS + JavaScript |
| 实时更新 V1 | HTTP Polling |
| 实时更新 V2 | SSE |
| 云端协议 | 后续 MQTT / HTTPS |
| 版本控制 | Git |

---

# 29. 核心开发原则

本项目始终遵守：

> **先跑通链路，再增加抽象；先保证稳定，再增加功能；先理解 ESP-IDF，再引入更多框架。**

核心链路始终是：

```text
GNSS → UART → ESP32-S3 → Wi-Fi → HTTP → Browser
```

只要这条链路稳定，后面的地图、VPS、MQTT、OTA、数据库都只是扩展。
