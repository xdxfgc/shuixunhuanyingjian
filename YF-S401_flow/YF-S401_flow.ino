// ============================================================
// 主文件：ESP32 + YF-S401 水流量传感器 + 继电器水泵 + WiFi HTTP 服务器
// 板子：ESP32 Dev Module
//
// 接线（详细说明见 README 和各模块顶部注释）：
//   YF-S401 红(VCC)->5V  黑(GND)->GND  黄(信号)->10kΩ 串到 D34，D34 再接 20kΩ 到 GND
//   WKY-1-RELAY-1 DC+->5V  DC-->GND  IN->D32  跳线帽拨到 H（高电平触发）
//   DS18B20 防水探头 红->3.3V  黑->GND  黄(信号)->D27（需 4.7kΩ 上拉到 3.3V）
//   DS18B20 #2 防水探头 红->3.3V  黑->GND  黄(信号)->D25（需 4.7kΩ 上拉到 3.3V）
//   加热继电器 IN->D26，线圈用独立电源，12V 加热模块接继电器 COM/NO
//   水泵用独立电源，接继电器 COM/NO，不要从 ESP32 的 5V 引脚取电
//
// 结构：
//   config.h          —— 引脚、常量、共享变量、模块接口声明
//   flow_sensor.cpp   —— 流量检测（D34 中断计数、结算）
//   relay.cpp         —— 继电器/水泵控制（D32）
//   heater.cpp        —— 加热继电器控制（D26）
//   temp_sensor.cpp   —— DS18B20 水温（D27，非阻塞读取）
//   temp_sensor2.cpp  —— DS18B20 水温 #2（D25，非阻塞读取）
//   web_server.cpp    —— 网页 + 所有 /api 接口
// ============================================================
#include "config.h"

// ---------------- 网络配置（在这里改） ----------------
// ⚠️ 标准 ESP32 只支持 2.4GHz WiFi，不支持 5GHz 频段！
const char* WIFI_SSID = "Xiaomi_AE4D";
const char* WIFI_PASSWORD = "123456780";

// 固定 IP（按你的路由器网段调整）
IPAddress LOCAL_IP(192, 168, 31, 100);
IPAddress GATEWAY(192, 168, 31, 1);
IPAddress SUBNET(255, 255, 255, 0);
IPAddress DNS1(192, 168, 31, 1);
IPAddress DNS2(223, 5, 5, 5);

// mDNS 域名，浏览器可访问 http://esp32flow.local
const char* MDNS_NAME = "esp32flow";

// ---------------- 全局共享变量（定义处） ----------------
volatile unsigned long pulseCount = 0;  // 中断累计脉冲数
float lastFlowRate = 0.0;                // 瞬时流量 L/min
float totalLiters = 0.0;                 // 累计水量 L
unsigned long lastPulseCount = 0;        // 上次结算时的脉冲计数
unsigned long lastSampleMs = 0;          // 上次结算时刻
unsigned long lastWindowMs = 0;          // 上次结算实际用时 ms
unsigned long lastWindowPulses = 0;      // 上次窗口内脉冲数
unsigned long startTime = 0;
bool pumpState = false;                  // 水泵当前状态：true=开
float pumpTargetLiters = 0.0;            // 定量浇水量 L
bool heaterState = false;                // 加热模块当前状态：true=开
float lastWaterTemp = NAN;               // 水温 ℃，无效时为 NAN
bool tempOk = false;                     // 温度读数是否有效
float lastWaterTemp2 = NAN;              // 水温2 ℃，无效时为 NAN
bool tempOk2 = false;                    // 温度2读数是否有效

WebServer server(80);

// ============================================================
// setup / loop
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  startTime = millis();

  // 模块初始化
  initFlowSensor();
  initRelay();
  initHeaterRelay();
  initTempSensor();
  initTempSensor2();

  // 连接 WiFi（固定 IP）
  WiFi.mode(WIFI_STA);
  WiFi.config(LOCAL_IP, GATEWAY, SUBNET, DNS1, DNS2);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("正在连接 WiFi: ");
  Serial.println(WIFI_SSID);

  unsigned long connectStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    // 30 秒连不上就重启，避免一直卡死
    if (millis() - connectStart > 30000) {
      Serial.println();
      Serial.println("WiFi 连接超时！请检查 SSID、密码以及是否为 2.4GHz 频段。");
      delay(30000);
      ESP.restart();
    }
  }

  Serial.println();
  Serial.print("连接成功！IP 地址：");
  Serial.println(WiFi.localIP());

  // mDNS：http://esp32flow.local
  if (MDNS.begin(MDNS_NAME)) {
    Serial.println("mDNS 已启用：http://" + String(MDNS_NAME) + ".local");
  }

  // 开机信号可能不稳定（D34 浮空 / 传感器未稳），等 1 秒后清零，忽略启动噪声
  delay(1000);
  resetTotal();
  Serial.println("开机噪声已清零，数据从 0 开始");

  // 注册并启动 HTTP 服务器
  registerRoutes();
  server.begin();
  Serial.println("HTTP 服务器已启动，端口 80");
}

void loop() {
  // 非阻塞轮询水温（每圈都调，内部自己控制节奏）
  processTempSensor();
  processTempSensor2();

  // 每 1 秒结算一次流量并更新缓存
  if (millis() - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    sampleFlow();

    // 串口监视器输出（115200）
    Serial.print("瞬时流量(L/min): ");
    Serial.print(lastFlowRate);
    Serial.print("  累计水量(L): ");
    Serial.print(totalLiters);
    Serial.print("  窗口脉冲数: ");
    Serial.print(lastWindowPulses);
    Serial.print("  窗口用时(ms): ");
    Serial.print(lastWindowMs);
    Serial.print("  水温(℃): ");
    Serial.print(tempOk ? String(lastWaterTemp, 1) : "无效");
    Serial.print("  水温2(℃): ");
    Serial.print(tempOk2 ? String(lastWaterTemp2, 1) : "无效");
    Serial.print("  水泵: ");
    Serial.print(pumpState ? "开" : "关");
    Serial.print("  加热: ");
    Serial.println(heaterState ? "开" : "关");
  }

  // 处理 HTTP 请求
  server.handleClient();
}
