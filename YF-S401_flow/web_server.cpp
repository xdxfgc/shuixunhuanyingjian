// ============================================================
// 网页 / HTTP 接口模块
// 负责：JSON 响应、状态页、所有 /api 接口、路由注册
// ============================================================
#include "config.h"

// 发送 JSON 响应（带 CORS 头，方便网页前端调用）
void sendJson(int code, const String& json) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(code, "application/json; charset=utf-8", json);
}

// GET /  —— 网页状态页（JS 定时刷新，点按钮不整页跳转）
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

// 注册所有路由
void registerRoutes() {
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
}
