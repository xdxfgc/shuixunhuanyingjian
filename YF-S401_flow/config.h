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

// 网页 / HTTP 模块
void sendJson(int code, const String& json);
void handleRoot();
void handleData();
void handleFlow();
void handleVolume();
void handleReset();
void handlePumpOn();
void handlePumpOff();
void handlePumpToggle();
void handlePumpState();
void handlePumpTarget();
void handleHealth();
void handleNotFound();
void registerRoutes();

#endif // YF_S401_CONFIG_H
