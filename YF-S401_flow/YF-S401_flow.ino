// ============================================================
// ESP32 + YF-S401 水流量传感器 + WiFi HTTP 服务器
// 板子：ESP32 Dev Module
//
// 接线（⚠️ YF-S401 必须 5V 供电，红线千万别接 3.3V）：
//   YF-S401 红（VCC）  -> ESP32 5V / VIN
//   YF-S401 黑（GND）  -> ESP32 GND          （必须和 ESP32 共地）
//   YF-S401 黄（信号） -> 10kΩ 串到 D34
//                         D34 再接 20kΩ 到 GND（分压，把 5V 高电平降到约 3.3V）
//   ⚠️ GPIO34(D34) 是输入专用脚，内部没有上拉电阻，
//     所以信号线必须用上面的分压，代码里 pinMode 用 INPUT 即可。
//   ⚠️ ESP32 GPIO 不是 5V 容忍，直接接 5V 信号会烧脚，务必加分压。
//   水管方向：箭头要和实际水流方向一致，装反数据会不准。
//
// 继电器水泵控制（WKY-1-RELAY-1，5V 光耦隔离继电器，高电平触发）：
//   RELAY DC+     -> ESP32 5V        （必须 5V，3.3V 吸合不了）
//   RELAY DC-     -> ESP32 GND       （必须共地，否则继电器乱跳）
//   RELAY IN      -> ESP32 GPIO32(D32)
//   板上跳线帽：采用高电平触发（H），代码里 RELAY_ACTIVE_HIGH 设为 1。
//   右侧 COM/NO 是水泵负载回路（注意！）：
//     水泵电源正 -> RELAY COM
//     RELAY NO   -> 水泵正极
//     水泵负极   -> 水泵电源负
//   ⚠️ 水泵电源用独立电源，绝对不要从 ESP32 的 5V 引脚取电！
//
// 功能：
//   1. 接入 WiFi，固定 IP
//   2. 启动 HTTP 服务器，其他设备可调用接口获取数据
//   3. 瞬时流量(L/min) + 累计过水总量(L)
//   4. 网页控制继电器水泵，定量浇水：累计水量达到设定值自动关泵
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ---------------- 网络配置（重点看这里） ----------------
// ⚠️ 标准 ESP32 只支持 2.4GHz WiFi，不支持 5GHz 频段！
// 这里填 2.4GHz 频段的 SSID，注意区分大小写。
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

// ---------------- YF-S401 流量传感器配置 ----------------
#define FLOW_PIN 34          // D34，信号输入（经分压后接入）
#define PULSES_PER_LITER 7.5 // YF-S401 系数：7.5 脉冲 = 1L

// 采样窗口：每 1 秒结算一次瞬时流量
const unsigned long SAMPLE_INTERVAL_MS = 1000;

// 中断里只能做最简单的事，计数器必须用 volatile
volatile unsigned long pulseCount = 0;

// ---------------- 继电器水泵控制（D32） ----------------
#define RELAY_PIN 32      // D32，继电器 IN（高电平触发）
#define RELAY_ACTIVE_HIGH 1  // 1=高电平触发(跳线帽在H)：HIGH吸合；0=低电平触发(跳线帽在L)
bool pumpState = false;        // 水泵当前状态：true=开
float pumpTargetLiters = 0.0;  // 定量浇水量 L，>0 时累计达到即自动关泵；0=不限

WebServer server(80);

// 缓存最近一次结果，接口直接读缓存，不阻塞、不频繁读
float lastFlowRate = 0.0;    // 瞬时流量 L/min
float totalLiters = 0.0;     // 累计水量 L
unsigned long lastPulseCount = 0;   // 上次结算时的脉冲计数
unsigned long lastSampleMs = 0;      // 上次结算时刻
unsigned long lastWindowMs = 0;      // 上次结算实际用时 ms
unsigned long lastWindowPulses = 0;  // 上次窗口内脉冲数
unsigned long startTime = 0;

// ============================================================
// 中断回调：只计数，越短越好
// ============================================================
void IRAM_ATTR pulseISR() {
  pulseCount++;
}

// ============================================================
// 工具函数
// ============================================================

// 读取当前脉冲计数（关中断再读，避免读到一半被 ISR 打断）
unsigned long readPulseCount() {
  noInterrupts();
  unsigned long val = pulseCount;
  interrupts();
  return val;
}

// 结算一次：根据窗口内脉冲数换算瞬时流量和累计量
void sampleFlow() {
  unsigned long now = millis();
  unsigned long cnt = readPulseCount();

  // 窗口用时（避免时钟回跳 / 首次为 0）
  unsigned long windowMs = (now >= lastSampleMs) ? (now - lastSampleMs) : 0;
  unsigned long delta = cnt - lastPulseCount;

  if (windowMs > 0) {
    float windowSec = windowMs / 1000.0;
    // 瞬时流量(L/min) = 窗口内脉冲数 / (7.5 * 窗口秒数)
    lastFlowRate = delta / (PULSES_PER_LITER * windowSec);
    // 累计水量 += 瞬时流量 * (窗口分钟数)
    totalLiters += lastFlowRate * (windowSec / 60.0);
  } else {
    lastFlowRate = 0.0;
  }

  lastWindowPulses = delta;
  lastWindowMs = windowMs;
  lastPulseCount = cnt;
  lastSampleMs = now;

  // 定量浇水：累计水量达到设定值，自动关泵
  if (pumpTargetLiters > 0 && totalLiters >= pumpTargetLiters) {
    if (pumpState) {
      setPump(false);
      Serial.println("已达到设定水量，自动关闭水泵");
    }
  }
}

// 清空累计过水总量
void resetTotal() {
  noInterrupts();
  pulseCount = 0;
  interrupts();
  lastPulseCount = 0;
  lastSampleMs = millis();
  totalLiters = 0.0;
  lastFlowRate = 0.0;
  lastWindowPulses = 0;
  lastWindowMs = 0;
}

// 控制水泵开关（高电平触发继电器）
void setPump(bool on) {
  pumpState = on;
#if RELAY_ACTIVE_HIGH
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
#else
  digitalWrite(RELAY_PIN, on ? LOW : HIGH);
#endif
  Serial.printf("水泵%s\n", on ? "已开启" : "已关闭");
}

// 发送 JSON 响应（带 CORS 头，方便网页前端调用）
void sendJson(int code, const String& json) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(code, "application/json; charset=utf-8", json);
}

// ============================================================
// 接口处理函数
// ============================================================

// GET /  —— 网页状态页，浏览器打开直接看
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                "<title>YF-S401 水流量检测</title>"
                "<style>body{font-family:sans-serif;padding:20px} "
                "b{font-size:20px} button{padding:8px 14px;margin:4px}</style>"
                "</head><body>";
  html += "<h1>YF-S401 水流量检测</h1>";
  html += "<p>瞬时流量：<b id='rate'>--</b> L/min</p>";
  html += "<p>累计水量：<b id='total'>--</b> L <span id='target'></span></p>";
  html += "<p>水泵状态：<b id='pump'>--</b></p>";
  html += "<button onclick=\"doPump(1)\">开水泵</button> ";
  html += "<button onclick=\"doPump(0)\">关水泵</button></p>";
  html += "<p>定量浇水(L)：<input id='tgt' type='number' step='0.1' min='0' value='0'> ";
  html += "<button onclick=\"doTarget()\">设定</button></p>";
  html += "<p>上一窗口脉冲数：<b id='pulses'>--</b>，用时 <b id='win'>--</b> ms</p>";
  html += "<button onclick=\"fetch('/api/reset').then(refresh)\">清零累计水量</button>";
  html += "<p><a href='/api/data'>查看 JSON 数据</a></p>";
  html += "<script>"
          "function refresh(){"
          "fetch('/api/data').then(function(r){return r.json()}).then(function(d){"
          "document.getElementById('rate').innerText=d.flowRate.toFixed(2);"
          "document.getElementById('total').innerText=d.totalLiters.toFixed(3);"
          "var p=document.getElementById('pump');"
          "p.innerText=d.pump?'运行中':'已停止';p.style.color=d.pump?'green':'red';"
          "document.getElementById('pulses').innerText=d.pulsesInLastWindow;"
          "document.getElementById('win').innerText=d.lastUpdateMs;"
          "if(d.pumpTarget>0){document.getElementById('target').innerText=' / 目标 '+d.pumpTarget.toFixed(2)+' L';}"
          "else{document.getElementById('target').innerText='';}"
          "}).catch(function(){});}"
          "function doPump(on){fetch('/api/pump/'+(on?'on':'off')).then(refresh);}"
          "function doTarget(){fetch('/api/pump/target?value='+document.getElementById('tgt').value).then(refresh);}"
          "refresh();setInterval(refresh,2000);"
          "</script></body></html>";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/html; charset=utf-8", html);
}

// GET /api/data —— 完整 JSON
void handleData() {
  String json = "{";
  json += "\"status\":\"ok\",";
  json += "\"flowRate\":" + String(lastFlowRate, 2) + ",";
  json += "\"totalLiters\":" + String(totalLiters, 3) + ",";
  json += "\"pulsesInLastWindow\":" + String(lastWindowPulses) + ",";
  json += "\"pump\":" + String(pumpState ? "true" : "false") + ",";
  json += "\"pumpTarget\":" + String(pumpTargetLiters, 2) + ",";
  json += "\"unit\":{\"flowRate\":\"L/min\",\"total\":\"L\"},";
  json += "\"lastUpdateMs\":" + String(millis() - lastSampleMs);
  json += "}";
  sendJson(200, json);
}

// GET /api/flow —— 瞬时流量
void handleFlow() {
  String json = "{\"value\":" + String(lastFlowRate, 2) + ",\"unit\":\"L/min\"}";
  sendJson(200, json);
}

// GET /api/volume —— 累计水量
void handleVolume() {
  String json = "{\"value\":" + String(totalLiters, 3) + ",\"unit\":\"L\"}";
  sendJson(200, json);
}

// GET /api/reset —— 清零累计水量
void handleReset() {
  resetTotal();
  String json = "{\"status\":\"ok\",\"totalLiters\":" + String(totalLiters, 3) + "}";
  sendJson(200, json);
}

// GET /api/pump/on —— 开水泵
void handlePumpOn() {
  setPump(true);
  sendJson(200, "{\"status\":\"ok\",\"pump\":true}");
}

// GET /api/pump/off —— 关水泵
void handlePumpOff() {
  setPump(false);
  sendJson(200, "{\"status\":\"ok\",\"pump\":false}");
}

// GET /api/pump/toggle —— 切换水泵开关
void handlePumpToggle() {
  setPump(!pumpState);
  String json = "{\"status\":\"ok\",\"pump\":" + String(pumpState ? "true" : "false") + "}";
  sendJson(200, json);
}

// GET /api/pump/state —— 查询水泵状态
void handlePumpState() {
  String json = "{\"pump\":" + String(pumpState ? "true" : "false") + "}";
  sendJson(200, json);
}

// GET /api/pump/target?value=2.5 —— 设置定量浇水量(L)；不带参数则返回当前设定
void handlePumpTarget() {
  if (server.hasArg("value")) {
    float v = server.arg("value").toFloat();
    if (v >= 0) {
      pumpTargetLiters = v;
      String json = "{\"status\":\"ok\",\"target\":" + String(pumpTargetLiters, 2) + "}";
      sendJson(200, json);
      return;
    }
  }
  String json = "{\"target\":" + String(pumpTargetLiters, 2) + "}";
  sendJson(200, json);
}

// GET /api/health —— 服务器状态
void handleHealth() {
  String json = "{";
  json += "\"status\":\"online\",";
  json += "\"uptimeMs\":" + String(millis() - startTime) + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"flowRate\":" + String(lastFlowRate, 2) + ",";
  json += "\"totalLiters\":" + String(totalLiters, 3) + ",";
  json += "\"pump\":" + String(pumpState ? "true" : "false") + ",";
  json += "\"pumpTarget\":" + String(pumpTargetLiters, 2);
  json += "}";
  sendJson(200, json);
}

// 404
void handleNotFound() {
  sendJson(404, "{\"status\":\"error\",\"message\":\"not found\"}");
}

// ============================================================
// setup / loop
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  startTime = millis();

  // 传感器信号输入（D34 无内部上拉，配合外部 10k+20k 分压，用 INPUT）
  pinMode(FLOW_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseISR, RISING);
  Serial.println("YF-S401 流量传感器初始化完成（D34，分压接入，上升沿计数）");

  // 继电器水泵：上电默认关闭（按 RELAY_ACTIVE_HIGH 极性）
  pinMode(RELAY_PIN, OUTPUT);
  setPump(false);
  Serial.println("继电器水泵初始化完成（D32，上电默认关闭）");

  lastPulseCount = readPulseCount();
  lastSampleMs = millis();

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

  // 注册接口
  server.on("/", handleRoot);
  server.on("/api/data", handleData);
  server.on("/api/flow", handleFlow);
  server.on("/api/volume", handleVolume);
  server.on("/api/reset", handleReset);
  server.on("/api/pump/on", handlePumpOn);
  server.on("/api/pump/off", handlePumpOff);
  server.on("/api/pump/toggle", handlePumpToggle);
  server.on("/api/pump/state", handlePumpState);
  server.on("/api/pump/target", handlePumpTarget);
  server.on("/api/health", handleHealth);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("HTTP 服务器已启动，端口 80");
}

void loop() {
  // 每 1 秒结算一次流量并更新缓存
  if (millis() - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    sampleFlow();

    // 串口监视器输出（任意波特率，代码里设 115200）
    Serial.print("瞬时流量(L/min): ");
    Serial.print(lastFlowRate);
    Serial.print("  累计水量(L): ");
    Serial.print(totalLiters);
    Serial.print("  窗口脉冲数: ");
    Serial.print(lastWindowPulses);
    Serial.print("  窗口用时(ms): ");
    Serial.print(lastWindowMs);
    Serial.print("  水泵: ");
    Serial.println(pumpState ? "开" : "关");
  }

  // 处理 HTTP 请求
  server.handleClient();
}
