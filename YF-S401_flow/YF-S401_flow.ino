// ============================================================
// 主文件：ESP32 + YF-S401 水流量传感器 + 继电器水泵 + WiFi HTTP 服务器
// 板子：ESP32 Dev Module
//
// 接线（详细说明见 README 和各模块顶部注释）：
//   YF-S401 红(VCC)->5V  黑(GND)->GND  黄(信号)->10kΩ 串到 D34，D34 再接 20kΩ 到 GND
//   WKY-1-RELAY-1 DC+->5V  DC-->GND  IN->D32  跳线帽拨到 H（高电平触发）
//   DS18B20 防水探头 红->3.3V  黑->GND  黄(信号)->D27（需 4.7kΩ 上拉到 3.3V）
//   DS18B20 #2 防水探头 红->3.3V  黑->GND  黄(信号)->D25（需 4.7kΩ 上拉到 3.3V）
//   压力传感器 红->5V  黑->GND  黄(信号)->10kΩ 串到 D33，D33 再接 20kΩ 到 GND
//   超声波测距 VCC->5V  GND->GND  Trig->D23  Echo->1kΩ->D18->2kΩ->GND（5V 必须分压）
//   GY-302 光照 VCC->3.3V  GND->GND  SDA->D21  SCL->D22
//   加热继电器 IN->D26，线圈用独立电源，12V 加热模块接继电器 COM/NO
//   第二路继电器（水泵2） DC+->5V  DC-->GND  IN->D14  跳线帽拨到 H
//   第二路加热继电器 DC+->5V  DC-->GND  IN->D16  跳线帽拨到 H（独立电源供加热负载）
//   水泵用独立电源，接继电器 COM/NO，不要从 ESP32 的 5V 引脚取电
//
// 结构：
//   config.h            —— 引脚、常量、共享变量、模块接口声明
//   flow_sensor.cpp     —— 流量检测（D34 中断计数、结算）
//   relay.cpp           —— 继电器/水泵控制（D32）
//   relay2.cpp          —— 第二路继电器/水泵控制（D14）
//   heater.cpp          —— 加热继电器控制（D26）
//   heater2.cpp         —— 第二路加热继电器控制（D16）
//   temp_sensor.cpp     —— DS18B20 水温（D27，非阻塞读取）
//   temp_sensor2.cpp    —— DS18B20 水温 #2（D25，非阻塞读取）
//   pressure_sensor.cpp —— 压力传感器（D33，模拟读取）
//   distance_sensor.cpp —— 超声波测距测水位（Trig D23 / Echo D18；水位 = 预定高度 − 测距）
//   light_sensor.cpp    —— GY-302 光照传感器（BH1750，I2C D21/D22）
//   web_server.cpp      —— 网页 + 所有 /api 接口
// ============================================================
#include "config.h"

// ---------------- 网络配置（在这里改） ----------------
// ⚠️ 标准 ESP32 只支持 2.4GHz WiFi，不支持 5GHz 频段！
const char* WIFI_SSID = "最优化太难复习了";
const char* WIFI_PASSWORD = "88888888";

// 固定 IP（按你的路由器网段调整）
IPAddress LOCAL_IP(10,177,222,100);
IPAddress GATEWAY(10,177,222,64);
IPAddress SUBNET(255, 255, 255, 0);
IPAddress DNS1(10,177,222,64);
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
unsigned long lastReconnectMs = 0;        // WiFi 断线重连计时
unsigned long lastWifiReportMs = 0;       // WiFi 状态打印计时
bool pumpState = false;                  // 水泵当前状态：true=开
bool pumpState2 = false;                 // 水泵2 当前状态：true=开（D14）
float pumpTargetLiters = 0.0;            // 定量浇水量 L
bool heaterState = false;                // 加热模块当前状态：true=开
bool heaterState2 = false;               // 加热2 当前状态：true=开（D16）
float lastWaterTemp = NAN;               // 水温 ℃，无效时为 NAN
bool tempOk = false;                     // 温度读数是否有效
float lastWaterTemp2 = NAN;              // 水温2 ℃，无效时为 NAN
bool tempOk2 = false;                    // 温度2读数是否有效
float lastPressure = 0.0;                // 压力 MPa
float lastPressureVoltage = 0.0;         // 传感器输出电压（标定用）
float lastPressureRaw = 0.0;             // 压力 ADC 原始平均值 0~4095（排查用）
float lastPressurePinVoltage = 0.0;      // D33 引脚实际电压 V（排查用）
bool pressureOk = false;                 // 压力读数是否有效
float lastDistanceMm = 0.0;              // 测距值 mm（传感器 → 水面）
float lastEchoUs = 0.0;                  // 回声脉冲宽度 µs（0=无回波，排查用）
bool distanceOk = false;                 // 测距读数是否有效
float tankHeightMm = TANK_HEIGHT_MM;     // ★预定高度：传感器出光面 → 箱底（水位基准）
float lastLevelMm = 0.0;                 // 水位高度 mm = 预定高度 − 测距值
float lastLevelPercent = 0.0;            // 水位 0~100 %
bool levelOk = false;                    // 水位是否有效
float lastLux = 0.0;                     // 光照强度 lx
bool lightOk = false;                    // 光照读数是否有效
unsigned long lastAdcDumpMs = 0;         // 接线自检输出计时（ADC_DEBUG_DUMP=1 时用）

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
  initRelay2();
  initHeaterRelay();
  initHeaterRelay2();
  initTempSensor();
  initTempSensor2();
  initPressureSensor();
  initDistanceSensor();
  initLightSensor();

  // 连接 WiFi（固定 IP）
  WiFi.mode(WIFI_STA);
  // ★ 关闭 WiFi 省电模式：默认省电模式在手机热点上会导致周期性掉线
  WiFi.setSleep(false);
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
  // 非阻塞轮询各传感器（每圈都调，各模块内部自己控制节奏）
  processTempSensor();
  processTempSensor2();
  processPressureSensor();
  processDistanceSensor();
  processLightSensor();

#if ADC_DEBUG_DUMP
  // 接线自检：打印空闲的 ADC1 引脚原始值，便于确认线插在哪个脚
  // ⚠️ 这里故意不读 D34（流量中断脚）——analogRead 会解除该脚的数字中断，导致流量读 0
  if (millis() - lastAdcDumpMs >= ADC_DEBUG_INTERVAL_MS) {
    lastAdcDumpMs = millis();
    Serial.print("[ADC自检] D33(压力)=");
    Serial.print(analogRead(33));
    Serial.print(" D35(空闲)=");
    Serial.print(analogRead(35));
    Serial.print(" D36(空闲)=");
    Serial.print(analogRead(36));
    Serial.print(" D39(空闲)=");
    Serial.println(analogRead(39));
  }
#endif

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
    Serial.print("  压力(MPa): ");
    Serial.print(pressureOk ? String(lastPressure, 3) : "无效");
    Serial.print(" [ADC ");
    Serial.print(lastPressureRaw, 0);
    Serial.print(" / 引脚 ");
    Serial.print(lastPressurePinVoltage, 3);
    Serial.print("V / 传感器 ");
    Serial.print(lastPressureVoltage, 3);
    Serial.print("V]");
    Serial.print("  水泵: ");
    Serial.print(pumpState ? "开" : "关");
    Serial.print("  水泵2: ");
    Serial.print(pumpState2 ? "开" : "关");
    Serial.print("  加热: ");
    Serial.print(heaterState ? "开" : "关");
    Serial.print("  加热2: ");
    Serial.print(heaterState2 ? "开" : "关");
    Serial.print("  水位(%): ");
    Serial.print(levelOk ? String(lastLevelPercent, 1) : "无效");
    Serial.print(" 高度: ");
    if (levelOk) {
      Serial.print(lastLevelMm, 1);
      Serial.print("mm");
    } else {
      Serial.print("无效");
    }
    Serial.print(" 测距: ");
    Serial.print(distanceOk ? String(lastDistanceMm, 1) : "无效");
    Serial.print("mm [回声 ");
    Serial.print(lastEchoUs, 0);
    Serial.print("us] 预定高度: ");
    Serial.print(tankHeightMm, 1);
    Serial.print("mm");
    Serial.print("  光照(lx): ");
    Serial.println(lightOk ? String(lastLux, 1) : "无效");
  }

  // ---- WiFi 掉线自动重连 + 状态监控 ----
  // 手机热点容易短暂断开：每 5 秒尝试重连一次，连上后每 10 秒报一次状态
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastReconnectMs >= 5000) {
      lastReconnectMs = millis();
      Serial.println("WiFi 断开，正在重连...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  } else if (millis() - lastWifiReportMs >= 10000) {
    lastWifiReportMs = millis();
    Serial.printf("[WiFi] 在线  IP=%s  信号=%d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }

  // 处理 HTTP 请求
  server.handleClient();
}
