# shuixunhuanyingjian

ESP32 + YF-S401 水流量传感器 + WiFi HTTP 服务器 + 继电器水泵 的定量浇水项目。

## 功能

- YF-S401 流量检测（D34）：瞬时流量（L/min）、累计过水总量（L）
- DS18B20 防水探头水温检测 ×2（D27 / D25，非阻塞读取）
- 压力传感器 0-1MPa 模拟输出检测（D33）
- 超声波测距 → 水位检测（Trig D23 / Echo D18）：水位 = 预定高度 − 测距值
- GY-302 / BH1750 光照检测（I2C D21/D22）
- WiFi 联网（固定 IP + mDNS）
- 网页控制继电器水泵（D32）开关
- 网页控制第二路继电器水泵（D14）开关
- 网页控制加热继电器（D26）开关
- 定量浇水：累计水量达到设定值自动关泵
- HTTP 接口，方便其他设备取数据 / 控制

## 接线（详见 .ino 顶部注释）

- YF-S401：红 → 5V，黑 → GND，黄（信号）→ 10kΩ 串到 D34，D34 再接 20kΩ 到 GND
- DS18B20 防水探头：红 → 3.3V，黑 → GND，黄（信号）→ D27（需 4.7kΩ 上拉到 3.3V）
- DS18B20 #2 防水探头：红 → 3.3V，黑 → GND，黄（信号）→ D25（需 4.7kΩ 上拉到 3.3V）
- 压力传感器（0-1MPa）：红 → 5V，黑 → GND，黄（信号）→ 10kΩ 串到 D33，D33 再接 20kΩ 到 GND
  （5V 供电时输出可达 4.5V，超过 ESP32 ADC 量程，必须分压；换算参数在 config.h 里）
- 超声波测距（HC-SR04 / JSN-SR04T）：VCC → 5V，GND → GND，Trig → D23，Echo → 1kΩ → D18 → 2kΩ → GND
  （Echo 输出 5V，必须分压；3.3V 版本模块如 RCWL-1601 可直连）
- GY-302 光照：VCC → 3.3V，GND → GND，SDA → D21，SCL → D22（模块自带上拉，无需外接）
- 继电器（WKY-1-RELAY-1）：DC+ → 5V，DC- → GND，IN → D32，跳线帽拨到 H（高电平触发）
- 第二路继电器（水泵2）：DC+ → 5V，DC- → GND，IN → **D14**，跳线帽拨到 H（高电平触发）
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
| `GET /api/temperature2` | 水温2（℃） |
| `GET /api/pressure` | 压力（MPa，附 voltage 标定值） |
| `GET /api/level` | 水位（% / mm，附测距值与回声脉宽） |
| `GET /api/level/height?value=250` | 设置/查询预定高度（mm，写入 NVS 掉电不丢） |
| `GET /api/light` | 光照强度（lx） |
| `GET /api/pump/on` | 开水泵 |
| `GET /api/pump/off` | 关水泵 |
| `GET /api/pump/toggle` | 切换 |
| `GET /api/pump/state` | 水泵状态 |
| `GET /api/pump/target?value=2.5` | 设定定量浇水量 |
| `GET /api/pump2/on` | 开第二路水泵（D14） |
| `GET /api/pump2/off` | 关第二路水泵 |
| `GET /api/pump2/toggle` | 切换第二路水泵 |
| `GET /api/pump2/state` | 第二路水泵状态 |
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
├── temp_sensor2.cpp   水温检测模块 #2（D25，DS18B20）
├── pressure_sensor.cpp 压力传感器模块（D33，0-1MPa 模拟）
├── distance_sensor.cpp 超声波测距/水位模块（Trig D23 / Echo D18）
├── light_sensor.cpp    GY-302 光照模块（BH1750，I2C D21/D22）
├── relay.cpp          继电器/水泵控制模块（D32）
├── relay2.cpp         第二路继电器/水泵控制模块（D14）
├── heater.cpp         加热继电器控制模块（D26）
└── web_server.cpp     网页 + 所有 /api 接口 + 路由
```

在 Arduino IDE 里打开整个 `YF-S401_flow` 文件夹，会显示成多个标签页，直接编译即可。

## 调试提示

`config.h` 里的 `ADC_DEBUG_DUMP` **默认为 `0`（关闭）**。改成 `1` 会每 5 秒把 D33/D35/D36/D39 的 ADC
原始值打到串口，用于确认「线到底插在哪个引脚」（用杜邦线把某个脚短到 GND 或 3.3V，看哪一列数字跟着变）。

⚠️ **自检列表里绝对不能加 D34**：D34 是流量传感器的中断脚，ESP32 的 `analogRead()` 会把该脚切到模拟模式
并解除数字中断，导致流量永远读 0。这个坑已经踩过一次，代码里已移除。

## 扩展文档：怎么再加一路同类模块

`添加传感器方案/` 目录下有 8 份操作文档，说明如何再增加一路流量、温度、压力、超声波水位、光照和继电器模块，
每份都包含引脚选择、接线、完整模块代码、config.h / 主文件 / web_server.cpp 的改动位置、测试方法和常见坑。

| 文档 | 加什么 | 新增接口 |
|------|--------|----------|
| `00_总览与引脚分配.md` | 索引 + 通用套路 + 引脚分配总表 | — |
| `增加第二路流量传感器-操作步骤.md` | 流量计 #2 | `/api/flow2`、`/api/volume2`、`/api/reset2` |
| `01_增加第二路温度传感器-操作步骤.md` | DS18B20 #3 | `/api/temperature3` |
| `02_增加第二路压力传感器-操作步骤.md` | 压力传感器 #2 | `/api/pressure2` |
| `03_增加第二路超声波水位-操作步骤.md` | 超声波 #2 | `/api/level2`、`/api/level2/height` |
| `04_增加第二路光照传感器-操作步骤.md` | GY-302 #2（共用 I2C 总线） | `/api/light2` |
| `05_增加第二路水泵继电器-操作步骤.md` | 水泵继电器 #2 | `/api/pump2/on` 等 4 个 |
| `06_增加第二路加热继电器-操作步骤.md` | 加热继电器 #2 | `/api/heater2/on` 等 4 个 |
