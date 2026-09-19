// ============================================================
// 配置文件：集中放引脚、常量、共享变量声明和模块接口
// 所有 .cpp 都 include 这个文件，避免跨文件重复、冲突
// ============================================================
#ifndef YF_S401_CONFIG_H
#define YF_S401_CONFIG_H

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
// ⚠️ 以下库虽然只在各个 .cpp 模块里用到，但必须集中写在这个文件里。
// 原因：Arduino 的库发现机制并不总能扫到「只被额外 .cpp 文件包含」的库，
// 一旦漏掉就会报 "xxx.h: No such file or directory"（Wire.h / Preferences.h /
// OneWire.h / DallasTemperature.h 都踩过这个坑）。
// config.h 被 .ino 直接包含，写在这里能被稳定发现。
#include <Wire.h>             // GY-302（BH1750）I2C
#include <Preferences.h>      // 液位标定值掉电保存（NVS）
#include <OneWire.h>          // DS18B20 单总线
#include <DallasTemperature.h> // DS18B20 温度读取
#include <esp_system.h>       // esp_reset_reason()（查上次复位原因）

// ---------------- 网络配置（在这里改 SSID/密码/IP） ----------------
// ⚠️ 标准 ESP32 只支持 2.4GHz WiFi，不支持 5GHz 频段！
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
extern IPAddress LOCAL_IP;
extern IPAddress GATEWAY;
extern IPAddress SUBNET;
extern IPAddress DNS1;
extern IPAddress DNS2;
extern const char* MDNS_NAME;

// ---------------- YF-S401 流量传感器 ----------------
#define FLOW_PIN 34               // D34，信号输入（经分压后接入）
#define PULSES_PER_LITER 7.5      // YF-S401 系数：7.5 脉冲 = 1L
#define SAMPLE_INTERVAL_MS 1000UL // 每 1 秒结算一次

extern volatile unsigned long pulseCount;  // 中断累计脉冲数
extern float lastFlowRate;                 // 瞬时流量 L/min
extern float totalLiters;                  // 累计水量 L
extern unsigned long lastPulseCount;       // 上次结算时的脉冲计数
extern unsigned long lastSampleMs;         // 上次结算时刻
extern unsigned long lastWindowMs;         // 上次结算实际用时 ms
extern unsigned long lastWindowPulses;     // 上次窗口内脉冲数
extern unsigned long startTime;
extern unsigned long wifiReconnectCount;  // WiFi 断线重连次数（诊断用）

// ---------------- 继电器水泵 ----------------
#define RELAY_PIN 32            // D32，继电器 IN
#define RELAY_ACTIVE_HIGH 1     // 1=高电平触发(跳线帽在H)；0=低电平触发(跳线帽在L)
extern bool pumpState;          // 水泵当前状态：true=开
extern float pumpTargetLiters;  // 定量浇水量 L，>0 时累计达到即自动关泵；0=不限

// ---------------- 继电器水泵 #2 ----------------
#define RELAY2_PIN 14           // D14，第二个继电器的 IN
#define RELAY2_ACTIVE_HIGH 1    // 1=高电平触发(跳线帽在H)；0=低电平触发(跳线帽在L)
extern bool pumpState2;         // 水泵2 当前状态：true=开

// ---------------- 电磁继电器 / 加热模块 ----------------
#define HEATER_PIN 26            // D26，加热继电器信号线 IN
#define HEATER_ACTIVE_HIGH 1     // 1=高电平触发(跳线帽在H)；0=低电平触发(跳线帽在L)
extern bool heaterState;         // 加热模块当前状态：true=开

// ---------------- 加热模块 #2 ----------------
#define HEATER2_PIN 16           // D16，第二个加热继电器信号线 IN
#define HEATER2_ACTIVE_HIGH 1    // 1=高电平触发(跳线帽在H)；0=低电平触发(跳线帽在L)
extern bool heaterState2;        // 加热2 当前状态：true=开

// ---------------- DS18B20 温度传感器 ----------------
#define TEMP_PIN 27                 // D27，数据线（需 4.7kΩ 上拉到 3.3V）
#define TEMP_READ_INTERVAL_MS 2000UL  // 每 2 秒请求一次转换
#define TEMP_CONVERSION_MS 750UL      // 12 位精度转换时间

extern float lastWaterTemp;       // 水温 ℃，无效时为 NAN
extern bool tempOk;               // 温度读数是否有效

// ---------------- DS18B20 温度传感器 #2 ----------------
#define TEMP2_PIN 25                  // D25，第二个探头数据线（需 4.7kΩ 上拉到 3.3V）
#define TEMP2_READ_INTERVAL_MS 2000UL // 每 2 秒请求一次转换
#define TEMP2_CONVERSION_MS 750UL     // 12 位精度转换时间

extern float lastWaterTemp2;      // 水温2 ℃，无效时为 NAN
extern bool tempOk2;              // 温度2读数是否有效

// ---------------- 压力传感器（0-1MPa 模拟输出，D33） ----------------
// ⚠️ 若传感器 5V 供电、输出 0.5~4.5V，D33 前必须分压（10kΩ 串 + 20kΩ 到 GND）
#define PRESSURE_PIN 33                  // D33，ADC1（WiFi 下也能用）
#define PRESSURE_SAMPLE_INTERVAL_MS 1000UL
#define PRESSURE_VREF 3.3                // ADC 满量程参考电压（衰减 11dB）
#define PRESSURE_V_MIN 0.5               // 0 MPa 时传感器输出电压
#define PRESSURE_V_MAX 4.5               // 满量程时传感器输出电压
#define PRESSURE_P_MAX 1.0               // 量程 MPa
#define PRESSURE_DIVIDER_RATIO 0.6667    // 10k 串 + 20k 到 GND：引脚电压 = 传感器输出 × 0.6667
#define PRESSURE_SAMPLES 16              // 每次采样平均次数

extern float lastPressure;           // 压力 MPa
extern float lastPressureVoltage;    // 换算出的传感器输出电压（标定用）
extern float lastPressureRaw;        // ADC 原始平均值（0~4095，标定/排查用）
extern float lastPressurePinVoltage; // D33 引脚实际电压 V（标定/排查用）
extern bool pressureOk;              // 压力读数是否有效

// ---------------- 超声波测距传感器（HC-SR04 / JSN-SR04T，Trig + Echo）----------------
// 原理：Trig 收 10µs 脉冲后发一次超声波，Echo 回一个「高电平宽度 = 声波往返时间」的脉冲：
//         距离 = 声速 × 时间 / 2    （空气中约 343m/s，即 0.343mm/µs）
//       传感器装在箱子上方朝下打水面，水位反着算：
//         水位 = 预定高度(TANK_HEIGHT_MM) − 测距值
//       箱子越空 → 测距越大 → 水位越低；水越满 → 测距越小 → 水位越高
//
// 接线：VCC -> 5V，GND -> GND，Trig -> D23，Echo -> 分压 -> D18
//   ⚠️ Echo 输出是 5V，必须分压后再进 ESP32：Echo →1kΩ→ D18，D18 →2kΩ→ GND
//      （用 10kΩ + 20kΩ 也行，比例同样是 0.667，输出电压约 3.3V）
//      若用的是 3.3V 版本模块（如 RCWL-1601），Echo 可以直连不用分压
#define ULTRASONIC_TRIG_PIN 23        // D23，Trig（数字输出）
#define ULTRASONIC_ECHO_PIN 18        // D18，Echo（数字输入，经分压）
#define ULTRASONIC_SAMPLE_INTERVAL_MS 500UL   // 每 0.5 秒测一次
#define ULTRASONIC_MIN_INTERVAL_MS 60UL       // HC-SR04 两次触发之间至少要隔 60ms
#define ULTRASONIC_EXTRA_TIMEOUT_MM 300.0     // 等待回波的余量（超出量程就不等，减少阻塞）
#define ULTRASONIC_MEDIAN_SAMPLES 3           // 最近 3 次有效读数取中位数，抗水波干扰

// ★ 预定高度（毫米）：传感器探头面 → 箱底 的垂直距离，水位计算的基准
//   本机预定高度 = 10cm = 100mm（探头面到箱底 100mm）
//   换箱子或挪传感器后重新量一次；也可运行时用 /api/level/height?value=100 修改（存 NVS，掉电不丢）
#define TANK_HEIGHT_MM 100.0

extern float lastDistanceMm;       // 测距值 mm（传感器 → 水面）
extern float lastEchoUs;           // 回声脉冲宽度 µs（0 = 没收到回波，排查用）
                                   // 换算：距离(mm) = 脉宽(µs) × 0.343 ÷ 2
extern bool distanceOk;            // 测距读数是否有效
extern float tankHeightMm;         // 当前预定高度 mm
extern float lastLevelMm;          // 水位高度 mm = 预定高度 − 测距值
extern float lastLevelPercent;     // 水位 0~100 %
extern bool levelOk;               // 水位是否有效（跟随测距是否有效）

// ---------------- 超声波测距 #2（Trig D4 / Echo D13）----------------
#define ULTRASONIC2_TRIG_PIN 4        // D4，Trig（数字输出）
#define ULTRASONIC2_ECHO_PIN 13       // D13，Echo（数字输入，经分压）
// 采样间隔、超时余量、中位数样本数复用第一路的宏

// ★ 水位2 的预定高度（毫米）：探头面 → 箱底
//   换箱子或用 /api/level2/height?value=100 修改（存 NVS，掉电不丢）
#define TANK_HEIGHT_MM2 100.0

extern float lastDistanceMm2;      // 测距值 mm（传感器2 → 水面）
extern float lastEchoUs2;          // 回声脉冲宽度 µs（0 = 没收到回波）
extern bool distanceOk2;           // 测距2是否有效
extern float tankHeightMm2;        // 水位2 当前预定高度 mm
extern float lastLevelMm2;         // 水位2 高度 mm
extern float lastLevelPercent2;    // 水位2 百分比
extern bool levelOk2;              // 水位2 是否有效

// ---------------- GY-302 光照传感器（BH1750 芯片，I2C） ----------------
// 接线：VCC -> 3.3V，GND -> GND，SDA -> D21，SCL -> D22（模块自带 4.7kΩ 上拉，无需外接）
// 地址：ADDR 悬空/接地 = 0x23（默认）；ADDR 接 VCC = 0x5C。初始化时会自动探测
#define LIGHT_SDA_PIN 21              // D21，I2C SDA
#define LIGHT_SCL_PIN 22              // D22，I2C SCL
#define LIGHT_ADDR_PRIMARY 0x23       // ADDR 悬空/接 GND
#define LIGHT_ADDR_ALT 0x5C           // ADDR 接 VCC
#define LIGHT_READ_INTERVAL_MS 1000UL // 高分辨率模式转换约 120ms，每秒读一次足够

extern float lastLux;             // 光照强度 lx，无效时无效标志为 false
extern bool lightOk;              // 光照读数是否有效

// ---------------- WebServer ----------------
extern WebServer server;

// ---------------- 接线自检（调试用） ----------------
// 1=每 5 秒把空闲的 ADC1 引脚原始值打到串口，用来确认「线到底插在哪个脚」：
//   [ADC自检] D33(压力)=.. D34(流量)=.. D35(液位)=.. D36(空闲)=.. D39(空闲)=..
// 用一根杜邦线把某个引脚短到 GND 或 3.3V，看哪一列数字跟着变，就知道引脚和线是否对应。
// 全部接线确认无误后改成 0，串口就只剩每秒的正常数据。
// ⚠️ 注意：自检里绝不能 analogRead(D34) —— D34 是流量传感器的中断脚，
//    ESP32 的 analogRead() 会把引脚切到模拟模式并解除数字中断，导致流量永远读 0。
//    所以自检只打 D33(压力) 和真正空闲的 D35/D36/D39。
//    接线确认无误后保持 0，串口就只剩每秒的正常数据。
#define ADC_DEBUG_DUMP 0
#define ADC_DEBUG_INTERVAL_MS 5000UL

// ---------------- 模块接口声明 ----------------
// 流量传感器模块
void initFlowSensor();
unsigned long readPulseCount();
void sampleFlow();
void resetTotal();

// 继电器模块
void initRelay();
void setPump(bool on);

// 第二路继电器模块
void initRelay2();
void setPump2(bool on);

// 加热继电器模块
void initHeaterRelay();
void setHeater(bool on);

// 第二路加热继电器模块
void initHeaterRelay2();
void setHeater2(bool on);

// 温度传感器模块
void initTempSensor();
void processTempSensor();
void initTempSensor2();
void processTempSensor2();

// 压力传感器模块
void initPressureSensor();
void processPressureSensor();

// 超声波测距 / 水位模块
void initDistanceSensor();
void processDistanceSensor();
void setTankHeightMm(float mm);
void initDistanceSensor2();
void processDistanceSensor2();
void setTankHeightMm2(float mm);

// 光照传感器模块（GY-302 / BH1750）
void initLightSensor();
void processLightSensor();

// 网页 / HTTP 模块
void sendJson(int code, const String& json);
const char* resetReasonText();   // 上次复位原因（诊断用）
void handleRoot();
void handleData();
void handleFlow();
void handleVolume();
void handleReset();
void handleTemperature();
void handleTemperature2();
void handlePressure();
void handlePumpOn();
void handlePumpOff();
void handlePumpToggle();
void handlePumpState();
void handlePumpTarget();
void handlePump2On();
void handlePump2Off();
void handlePump2Toggle();
void handlePump2State();
void handleHeaterOn();
void handleHeaterOff();
void handleHeaterToggle();
void handleHeaterState();
void handleHeater2On();
void handleHeater2Off();
void handleHeater2Toggle();
void handleHeater2State();
void handleHealth();
void handleLevel();
void handleLevelHeight();
void handleLevel2();
void handleLevelHeight2();
void handleLight();
void handleNotFound();
void registerRoutes();

#endif // YF_S401_CONFIG_H
