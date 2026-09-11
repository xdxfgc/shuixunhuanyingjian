# shuixunhuanyingjian

ESP32 + YF-S401 水流量传感器 + WiFi HTTP 服务器 + 继电器水泵 的定量浇水项目。

## 功能

- YF-S401 流量检测（D34）：瞬时流量（L/min）、累计过水总量（L）
- DS18B20 防水探头水温检测（D27，非阻塞读取）
- WiFi 联网（固定 IP + mDNS）
- 网页控制继电器水泵（D32）开关
- 网页控制加热继电器（D26）开关
- 定量浇水：累计水量达到设定值自动关泵
- HTTP 接口，方便其他设备取数据 / 控制

## 接线（详见 .ino 顶部注释）

- YF-S401：红 → 5V，黑 → GND，黄（信号）→ 10kΩ 串到 D34，D34 再接 20kΩ 到 GND
- DS18B20 防水探头：红 → 3.3V，黑 → GND，黄（信号）→ D27（需 4.7kΩ 上拉到 3.3V）
- 继电器（WKY-1-RELAY-1）：DC+ → 5V，DC- → GND，IN → D32，跳线帽拨到 H（高电平触发）
- 水泵：用独立电源，接继电器 COM/NO
- 加热继电器：IN → D26；线圈用独立电源；12V 加热模块接继电器 COM/NO

依赖库：OneWire、DallasTemperature（Arduino IDE 库管理器安装）

## HTTP 接口

| 接口 | 说明 |
|------|------|
| `GET /` | 中文控制网页 |
| `GET /api/data` | 完整 JSON |
| `GET /api/flow` | 瞬时流量 |
| `GET /api/volume` | 累计水量 |
| `GET /api/reset` | 清零累计水量 |
| `GET /api/temperature` | 水温（℃） |
| `GET /api/pump/on` | 开水泵 |
| `GET /api/pump/off` | 关水泵 |
| `GET /api/pump/toggle` | 切换 |
| `GET /api/pump/state` | 水泵状态 |
| `GET /api/pump/target?value=2.5` | 设定定量浇水量 |
| `GET /api/heater/on` | 开加热 |
| `GET /api/heater/off` | 关加热 |
| `GET /api/heater/toggle` | 切换加热 |
| `GET /api/heater/state` | 加热状态 |
| `GET /api/health` | 服务器状态 |

用 Arduino IDE 打开 `YF-S401_flow/YF-S401_flow.ino`，选择 ESP32 Dev Module 编译烧录即可。

## 代码结构（模块化）

```
YF-S401_flow/
├── YF-S401_flow.ino   主文件：网络配置、全局变量、WiFi 连接、setup/loop
├── config.h           配置文件：引脚、常量、共享变量声明、模块接口
├── flow_sensor.cpp    流量检测模块（D34 中断计数、结算）
├── temp_sensor.cpp    水温检测模块（D27，DS18B20）
├── relay.cpp          继电器/水泵控制模块（D32）
├── heater.cpp         加热继电器控制模块（D26）
└── web_server.cpp     网页 + 所有 /api 接口 + 路由
```

在 Arduino IDE 里打开整个 `YF-S401_flow` 文件夹，会显示成多个标签页，直接编译即可。
