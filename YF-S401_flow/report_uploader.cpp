// ============================================================
// 数据主动上报模块（推送到后端 /api/water/ingest）
//
// 背景：后端默认每秒轮询本机（每次要等 0.6~1.9s），本模块改成由 ESP32
//       主动推数据，每 1.5 秒推一次，后端收到后 2 秒内用推送值覆盖轮询值。
//
// 流程：
//   ① 登录  POST /api/auth/login  {username, password}  → 拿 token（有效期 12h）
//   ② 上报  POST /api/water/ingest + Header: Authorization: Bearer <token>
//   ③ 若返回 40101（token 过期）→ 自动重新登录，下一轮继续推
//
// ⚠️ 压力单位：后端要 kPa，本机传感器输出 MPa，上报时 ×1000
// ⚠️ 后端用 HTTP 200 返回业务码，所以必须解析返回 JSON 里的 "code"，不能只看状态码
// ============================================================
#include "config.h"
#include <WiFiClient.h>
#include <HTTPClient.h>

// ---------------- 后端配置（要改就改这里） ----------------
static const char* BACKEND_HOST = "http://10.177.222.67:8000";
static const char* LOGIN_PATH   = "/api/auth/login";
static const char* INGEST_PATH  = "/api/water/ingest";
static const char* BACKEND_USER = "admin";
static const char* BACKEND_PASS = "admin123";

// ---------------- 上报参数 ----------------
static const unsigned long UPLOAD_PERIOD_MS  = 1500;  // 上报周期 1.5 秒
static const unsigned long UPLOAD_PERIOD_FAIL_MS = 5000; // 连续失败后放慢到 5 秒（避免拖死循环）
static const int CONNECT_TIMEOUT_MS          = 500;   // 建连接超时
static const int RESPONSE_TIMEOUT_MS         = 800;   // 等响应超时

// ---------------- 对外状态（/api/health 可查） ----------------
unsigned long uploadOkCount = 0;        // 成功次数
unsigned long uploadFailCount = 0;      // 失败次数
bool lastUploadOk = false;              // 上次是否成功
unsigned long lastUploadOkMs = 0;       // 上次成功时刻
String lastUploadMsg = "尚未上报";      // 上次结果说明
bool backendTokenOk = false;            // 是否已拿到后端 token

static String authToken = "";           // 当前 token
static unsigned long lastTryMs = 0;     // 上次尝试时刻
static unsigned long failStreak = 0;    // 连续失败次数（用于退避）

// ---------------- 登录，拿 token ----------------
static bool backendLogin() {
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, String(BACKEND_HOST) + LOGIN_PATH)) {
    lastUploadMsg = "登录初始化失败";
    return false;
  }
  http.setConnectTimeout(CONNECT_TIMEOUT_MS);
  http.setTimeout(RESPONSE_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");

  String body = String("{\"username\":\"") + BACKEND_USER +
                "\",\"password\":\"" + BACKEND_PASS + "\"}";
  int httpCode = http.POST(body);
  String resp = http.getString();
  http.end();

  if (httpCode != 200) {
    lastUploadMsg = "登录失败 HTTP " + String(httpCode);
    return false;
  }

  // 简单字符串解析出 token（避免再装一个 JSON 库）
  int k = resp.indexOf("\"token\":\"");
  if (k < 0) {
    lastUploadMsg = "登录返回里没有 token";
    return false;
  }
  k += 9;
  int e = resp.indexOf('"', k);
  if (e < 0) {
    lastUploadMsg = "token 解析失败";
    return false;
  }
  authToken = resp.substring(k, e);
  backendTokenOk = true;
  Serial.printf("[上报] 登录成功，token 长度 %u\n", (unsigned)authToken.length());
  return true;
}

// ---------------- 上报一次 ----------------
static bool uploadOnce() {
  if (authToken.length() == 0) {
    backendTokenOk = false;
    if (!backendLogin()) {
      uploadFailCount++;
      return false;
    }
  }

  // 组装 JSON：字段名和后端一致
  //   储水槽温度 = 水温2（D25）   加热槽温度 = 水温1（D27）
  //   压力 MPa → kPa（×1000）
  String payload = "{";
  payload += "\"flow_rate\":"    + String(lastFlowRate, 2) + ",";
  payload += "\"total_liters\":" + String(totalLiters, 3) + ",";
  payload += "\"storage_temp\":" + (tempOk2 ? String(lastWaterTemp2, 1) : String("null")) + ",";
  payload += "\"heater_temp\":"  + (tempOk  ? String(lastWaterTemp, 1)  : String("null")) + ",";
  payload += "\"pressure\":"     + (pressureOk ? String(lastPressure * 1000.0, 1) : String("null")) + ",";
  payload += "\"light\":"        + (lightOk ? String(lastLux, 1) : String("null"));
  payload += "}";

  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, String(BACKEND_HOST) + INGEST_PATH)) {
    lastUploadMsg = "上报初始化失败";
    uploadFailCount++;
    return false;
  }
  http.setConnectTimeout(CONNECT_TIMEOUT_MS);
  http.setTimeout(RESPONSE_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + authToken);

  int httpCode = http.POST(payload);
  String resp = http.getString();
  http.end();

  // 后端用 HTTP 200 返回业务码，必须看 JSON 里的 "code"
  bool ok = (httpCode == 200) && (resp.indexOf("\"code\":0") >= 0);
  if (ok) {
    uploadOkCount++;
    lastUploadOk = true;
    lastUploadOkMs = millis();
    lastUploadMsg = "ok";
  } else {
    uploadFailCount++;
    lastUploadOk = false;
    if (resp.indexOf("40101") >= 0 || httpCode == 401) {
      authToken = "";            // token 过期：清掉，下一轮重新登录
      backendTokenOk = false;
      lastUploadMsg = "token 过期，将重新登录";
    } else {
      lastUploadMsg = "HTTP " + String(httpCode) + " " + resp.substring(0, 30);
    }
  }
  return ok;
}

// ---------------- 模块接口 ----------------
void initUploader() {
  authToken = "";
  backendTokenOk = false;
  lastUploadMsg = "等待首次上报";
  lastTryMs = millis();          // 开机后隔一个周期再开始
  failStreak = 0;
  Serial.println("数据上报模块就绪（每 1.5 秒推送到后端）");
}

// 主循环每圈调用一次，内部自己控制节奏（不阻塞网页太久）
void processUploader() {
  // 连续失败 3 次以上就放慢到 5 秒一次，避免后端不通时把 loop 占满
  unsigned long period = (failStreak >= 3) ? UPLOAD_PERIOD_FAIL_MS : UPLOAD_PERIOD_MS;
  if (millis() - lastTryMs < period) return;
  lastTryMs = millis();

  // WiFi 没连上就跳过本次，不浪费超时时间，也不计失败
  if (WiFi.status() != WL_CONNECTED) {
    lastUploadMsg = "WiFi 未连接，跳过本次";
    return;
  }

  bool ok = uploadOnce();
  if (ok) {
    failStreak = 0;
  } else {
    failStreak++;
  }

  // 打印策略：失败必打，成功每 20 次打一次，避免刷屏
  static unsigned long okPrinted = 0;
  if (!ok) {
    Serial.printf("[上报] 失败：%s（累计成功 %lu / 失败 %lu）\n",
                  lastUploadMsg.c_str(), uploadOkCount, uploadFailCount);
  } else if (++okPrinted % 20 == 0) {
    Serial.printf("[上报] 正常，累计成功 %lu 次\n", uploadOkCount);
  }
}
