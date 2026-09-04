# shuixunhuanyingjian

ESP32 + YF-S401 水流量传感器 + WiFi HTTP 服务器 + 继电器水泵 的定量浇水项目。

## 功能

- YF-S401 流量检测（D34）：瞬时流量（L/min）、累计过水总量（L）
- WiFi 联网（固定 IP + mDNS）
- 网页控制继电器水泵（D32）开关
- 定量浇水：累计水量达到设定值自动关泵
- HTTP 接口，方便其他设备取数据 / 控制

## 接线（详见 .ino 顶部注释）

- YF-S401：红 → 5V，黑 → GND，黄（信号）→ 10kΩ 串到 D34，D34 再接 20kΩ 到 GND
- 继电器（WKY-1-RELAY-1）：DC+ → 5V，DC- → GND，IN → D32，跳线帽拨到 H（高电平触发）
- 水泵：用独立电源，接继电器 COM/NO

## HTTP 接口

| 接口 | 说明 |
|------|------|
| `GET /` | 中文控制网页 |
| `GET /api/data` | 完整 JSON |
| `GET /api/flow` | 瞬时流量 |
| `GET /api/volume` | 累计水量 |
| `GET /api/reset` | 清零累计水量 |
| `GET /api/pump/on` | 开水泵 |
| `GET /api/pump/off` | 关水泵 |
| `GET /api/pump/toggle` | 切换 |
| `GET /api/pump/state` | 水泵状态 |
| `GET /api/pump/target?value=2.5` | 设定定量浇水量 |
| `GET /api/health` | 服务器状态 |

用 Arduino IDE 打开 `YF-S401_flow/YF-S401_flow.ino`，选择 ESP32 Dev Module 编译烧录即可。
