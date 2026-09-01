# 硬件接线

## 器件

| 器件 | 型号 |
|---|---|
| 开发板 | YD-ESP32-S3 V1.3（源地 VCC-GND Studio） |
| 模组 | ESP32-S3-WROOM-1（板载 PCB 天线） |
| GNSS | SkyTraq S1216F8-BD |

## 开发板要点

- 两个 Type-C：一个接板载 CH343P USB 转串口（烧录用），一个是 ESP32-S3 原生 USB（GPIO19/20）
- 日志走 UART0：TX=GPIO43、RX=GPIO44，波特率 115200，经 CH343P 输出
- 板载 WS2812 RGB LED 占用 GPIO48
- **GPIO35 / GPIO36 / GPIO37 被模组内部 flash/PSRAM 占用，外部不可用**
- 其余 GPIO 均已引出到两侧排针

## 实际接线（已确认）

| GNSS | ESP32-S3 | 说明 |
|---|---|---|
| VCC | 3V3 | 3.3 V 供电 |
| GND | GND | 必须共地 |
| TX | GPIO18 | ESP32 接收，GPIO18 正是 U1RXD |
| RX | GPIO17 | ESP32 发送，GPIO17 正是 U1TXD，配置模块时使用 |
| PPS | GPIO16 | 已接，Phase 10 精确授时再用 |

GPIO17/18 是 ESP32-S3 的 UART1 原生引脚，配合 `GNSS_UART_NUM=1` 无需额外走 GPIO 矩阵。

## 默认引脚（menuconfig 可改）

| 配置项 | 默认 | 说明 |
|---|---|---|
| `GNSS_UART_NUM` | 1 | UART 端口 |
| `GNSS_UART_BAUDRATE` | 115200 | **本模块实测值**，见下文 |
| `GNSS_UART_RX_GPIO` | 18 | ESP32 RX ← GNSS TX |
| `GNSS_UART_TX_GPIO` | 17 | ESP32 TX → GNSS RX |

修改方式：

```bash
idf.py menuconfig
# → GNSS Configuration
```

## 波特率实测记录

数据手册写 9600，但这台 S1216F8-BD 实际跑在 **115200 8N1**。
上电自动探测（按可打印 ASCII 占比打分）的结果：

| 波特率 | 可打印 ASCII |
|---|---|
| 9600 | 33/128 |
| 38400 | 57/128 |
| **115200** | **128/128** |
| 57600 | 70/128 |
| 230400 | 15/128 |

另外该模块默认只输出 `$GNRMC`，更新率 **20 Hz**（时间戳按 0.05 s 步进）。
若 Phase 2 需要 GGA（卫星数、海拔），可能要下发配置命令开启，届时再处理。

## 检查清单

- [x] 确认开发板型号与引脚可用性
- [x] 共地：GNSS GND 与 ESP32 GND 已连接
- [x] 上电后在串口看到 `$GNRMC`（Phase 1 验收，115200 波特率）
- [ ] 确认开发板 3.3 V 供电电流是否满足 GNSS 模块峰值需求
- [ ] 天线朝上，初次冷启动定位需要开阔天空
