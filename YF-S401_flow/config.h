// ============================================================
// 配置文件：集中放引脚、常量、共享变量声明和模块接口
// 所有 .cpp 都 include 这个文件，避免跨文件重复、冲突
// ============================================================
#ifndef YF_S401_CONFIG_H
#define YF_S401_CONFIG_H

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

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

// ---------------- 继电器水泵 ----------------
#define RELAY_PIN 32            // D32，继电器 IN
#define RELAY_ACTIVE_HIGH 1     // 1=高电平触发(跳线帽在H)；0=低电平触发(跳线帽在L)
extern bool pumpState;          // 水泵当前状态：true=开
extern float pumpTargetLiters;  // 定量浇水量 L，>0 时累计达到即自动关泵；0=不限

// ---------------- 电磁继电器 / 加热模块 ----------------
#define HEATER_PIN 26            // D26，加热继电器信号线 IN
#define HEATER_ACTIVE_HIGH 1     // 1=高电平触发(跳线帽在H)；0=低电平触发(跳线帽在L)
extern bool heaterState;         // 加热模块当前状态：true=开

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
extern bool pressureOk;              // 压力读数是否有效

// ---------------- WebServer ----------------
extern WebServer server;

// ---------------- 模块接口声明 ----------------
// 流量传感器模块
void initFlowSensor();
unsigned long readPulseCount();
void sampleFlow();
void resetTotal();

// 继电器模块
void initRelay();
void setPump(bool on);

// 加热继电器模块
void initHeaterRelay();
void setHeater(bool on);

// 温度传感器模块
void initTempSensor();
void processTempSensor();
void initTempSensor2();
void processTempSensor2();

// 压力传感器模块
void initPressureSensor();
void processPressureSensor();

// 网页 / HTTP 模块
void sendJson(int code, const String& json);
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
void handleHeaterOn();
void handleHeaterOff();
void handleHeaterToggle();
void handleHeaterState();
void handleHealth();
void handleNotFound();
void registerRoutes();

#endif // YF_S401_CONFIG_H
