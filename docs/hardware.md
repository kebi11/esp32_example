# 硬件接线

## 器件

| 器件 | 型号 |
|---|---|
| MCU | ESP32-S3 |
| GNSS | SkyTraq S1216F8-BD |

## 第一阶段接线（只接收）

```text
GNSS VCC  -> ESP32 3.3V
GNSS GND  -> ESP32 GND
GNSS TXD  -> ESP32 RX (GPIO18)
```

## 需要配置模块时再接

```text
GNSS RXD <- ESP32 TX (GPIO17)
```

PPS 暂时保留，Phase 10 精确授时再处理。

## 默认引脚（menuconfig 可改）

| 配置项 | 默认 | 说明 |
|---|---|---|
| `GNSS_UART_NUM` | 1 | UART 端口 |
| `GNSS_UART_BAUDRATE` | 9600 | S1216F8-BD 出厂默认 |
| `GNSS_UART_RX_GPIO` | 18 | ESP32 RX ← GNSS TXD |
| `GNSS_UART_TX_GPIO` | 17 | ESP32 TX → GNSS RXD |

修改方式：

```bash
idf.py menuconfig
# → GNSS Configuration
```

## 检查清单

- [ ] 确认开发板 3.3V 供电电流是否满足 GNSS 模块峰值需求
- [ ] 共地：GNSS GND 与 ESP32 GND 必须连接
- [ ] 天线朝上，初次冷启动定位需要开阔天空
- [ ] 用 USB-TTL 直连 GNSS TXD 先确认能输出 `$GNGGA` / `$GNRMC`
