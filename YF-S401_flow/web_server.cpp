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
  html += "<p>水温：<b id='temp'>--</b> ℃</p>";
  html += "<p>水温2：<b id='temp2'>--</b> ℃</p>";
  html += "<p>压力：<b id='pressure'>--</b> MPa</p>";
  html += "<p>水位：<b id='level'>--</b> % / <b id='levelmm'>--</b> cm <span id='leveldist'></span> <span id='levelwarn' style='color:red'></span></p>";
  html += "<p>光照：<b id='light'>--</b> lx </p>";
  html += "<p>水泵状态：<b id='pump'>--</b></p>";
  html += "<button onclick=\"doPump(1)\">开水泵</button> ";
  html += "<button onclick=\"doPump(0)\">关水泵</button></p>";
  html += "<p>加热状态：<b id='heater'>--</b></p>";
  html += "<button onclick=\"doHeater(1)\">开加热</button> ";
  html += "<button onclick=\"doHeater(0)\">关加热</button></p>";
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
          "var t=document.getElementById('temp');"
          "t.innerText=(d.temperature!=null)?d.temperature.toFixed(1):'--';"
          "var t2=document.getElementById('temp2');"
          "t2.innerText=(d.temperature2!=null)?d.temperature2.toFixed(1):'--';"
          "var pr=document.getElementById('pressure');"
          "pr.innerText=(d.pressure!=null)?d.pressure.toFixed(3):'--';"
          "var lv=document.getElementById('level');"
          "lv.innerText=(d.level!=null)?d.level.toFixed(1):'--';"
          "var lm=document.getElementById('levelmm');"
          "lm.innerText=(d.levelMm!=null)?(d.levelMm/10).toFixed(1):'--';"
          "var ld=document.getElementById('leveldist');"
          "ld.innerText=(d.distanceMm!=null)?('（测距 '+(d.distanceMm/10).toFixed(1)+' cm）'):'';"
          "ld.style.color='gray';"
          "var lw=document.getElementById('levelwarn');"
          "lw.innerText=(d.ok===false)?'（测距无效：检查 Echo 分压接线/传感器朝向/预定高度）':'';"
          "var lx=document.getElementById('light');"
          "lx.innerText=(d.light!=null)?d.light.toFixed(0):'--';"
          "var p=document.getElementById('pump');"
          "p.innerText=d.pump?'运行中':'已停止';p.style.color=d.pump?'green':'red';"
          "var h=document.getElementById('heater');"
          "h.innerText=d.heater?'加热中':'已停止';h.style.color=d.heater?'orange':'gray';"
          "document.getElementById('pulses').innerText=d.pulsesInLastWindow;"
          "document.getElementById('win').innerText=d.lastUpdateMs;"
          "if(d.pumpTarget>0){document.getElementById('target').innerText=' / 目标 '+d.pumpTarget.toFixed(2)+' L';}"
          "else{document.getElementById('target').innerText='';}"
          "}).catch(function(){});}"
          "function doPump(on){fetch('/api/pump/'+(on?'on':'off')).then(refresh);}"
          "function doHeater(on){fetch('/api/heater/'+(on?'on':'off')).then(refresh);}"
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
  json += "\"temperature\":" + String(tempOk ? String(lastWaterTemp, 1) : String("null")) + ",";
  json += "\"tempOk\":" + String(tempOk ? "true" : "false") + ",";
  json += "\"temperature2\":" + String(tempOk2 ? String(lastWaterTemp2, 1) : String("null")) + ",";
  json += "\"tempOk2\":" + String(tempOk2 ? "true" : "false") + ",";
  json += "\"pressure\":" + String(pressureOk ? String(lastPressure, 3) : String("null")) + ",";
  json += "\"pressureOk\":" + String(pressureOk ? "true" : "false") + ",";
  json += "\"level\":" + String(levelOk ? String(lastLevelPercent, 1) : String("null")) + ",";
  json += "\"levelOk\":" + String(levelOk ? "true" : "false") + ",";
  json += "\"levelMm\":" + String(levelOk ? String(lastLevelMm, 1) : String("null")) + ",";
  json += "\"distanceMm\":" + String(distanceOk ? String(lastDistanceMm, 1) : String("null")) + ",";
  json += "\"distanceOk\":" + String(distanceOk ? "true" : "false") + ",";
  json += "\"tankHeightMm\":" + String(tankHeightMm, 1) + ",";
  json += "\"echoUs\":" + String(lastEchoUs, 0) + ",";
  json += "\"light\":" + String(lightOk ? String(lastLux, 1) : String("null")) + ",";
  json += "\"lightOk\":" + String(lightOk ? "true" : "false") + ",";
  json += "\"pump\":" + String(pumpState ? "true" : "false") + ",";
  json += "\"pumpTarget\":" + String(pumpTargetLiters, 2) + ",";
  json += "\"heater\":" + String(heaterState ? "true" : "false") + ",";
  json += "\"unit\":{\"flowRate\":\"L/min\",\"total\":\"L\",\"temperature\":\"C\",\"pressure\":\"MPa\",\"level\":\"%\",\"light\":\"lx\"},";
  json += "\"lastUpdateMs\":" + String(millis() - lastSampleMs);
  json += "}";
  sendJson(200, json);
}

// GET /api/level —— 水位（由超声波测距换算：水位 = 预定高度 − 测距值）
void handleLevel() {
  // 即使测距无效也带出回声脉宽，方便排查接线和安装
  String json = "{\"value\":" + String(levelOk ? String(lastLevelPercent, 1) : String("null")) +
                ",\"unit\":\"%\"," +
                "\"levelMm\":" + String(levelOk ? String(lastLevelMm, 1) : String("null")) + "," +
                "\"levelCm\":" + String(levelOk ? String(lastLevelMm / 10.0, 2) : String("null")) + "," +
                "\"distanceMm\":" + String(distanceOk ? String(lastDistanceMm, 1) : String("null")) + "," +
                "\"tankHeightMm\":" + String(tankHeightMm, 1) + "," +
                "\"ok\":" + String(levelOk ? "true" : "false") + "," +
                "\"distanceOk\":" + String(distanceOk ? "true" : "false") + "," +
                "\"echoUs\":" + String(lastEchoUs, 0) + "}";
  sendJson(levelOk ? 200 : 503, json);
}

// GET /api/level/height?value=250 —— 设置「预定高度」（毫米），写入 NVS 掉电不丢
//   预定高度 = 传感器出光面 → 箱底 的垂直距离；水位 = 预定高度 − 测距值
//   不带参数则返回当前值
void handleLevelHeight() {
  if (server.hasArg("value")) {
    float v = server.arg("value").toFloat();
    // 10~5000mm，超出多半是单位写错（比如填了 cm 或 m）
    if (v >= 10.0 && v <= 5000.0) {
      setTankHeightMm(v);
      String json = "{\"status\":\"ok\",\"tankHeightMm\":" + String(tankHeightMm, 1) + "}";
      sendJson(200, json);
      return;
    }
    sendJson(400, "{\"status\":\"error\",\"message\":\"value must be 10..5000 (mm)\"}");
    return;
  }
  String json = "{\"status\":\"ok\",\"tankHeightMm\":" + String(tankHeightMm, 1) +
                ",\"hint\":\"用法 /api/level/height?value=250（单位毫米，= 传感器到箱底的距离）\"}";
  sendJson(200, json);
}

// GET /api/light —— 光照强度（GY-302 / BH1750，单位 lx）
void handleLight() {
  String json = "{\"value\":" + String(lightOk ? String(lastLux, 1) : String("null")) +
                ",\"unit\":\"lx\"}";
  sendJson(lightOk ? 200 : 503, json);
}

// GET /api/temperature —— 水温
void handleTemperature() {
  if (!tempOk) {
    sendJson(503, "{\"status\":\"error\",\"message\":\"sensor read failed\"}");
    return;
  }
  String json = "{\"value\":" + String(lastWaterTemp, 1) + ",\"unit\":\"C\"}";
  sendJson(200, json);
}

// GET /api/temperature2 —— 水温2（第二个探头）
void handleTemperature2() {
  if (!tempOk2) {
    sendJson(503, "{\"status\":\"error\",\"message\":\"sensor read failed\"}");
    return;
  }
  String json = "{\"value\":" + String(lastWaterTemp2, 1) + ",\"unit\":\"C\"}";
  sendJson(200, json);
}

// GET /api/flow —— 瞬时流量
void handleFlow() {
  String json = "{\"value\":" + String(lastFlowRate, 2) + ",\"unit\":\"L/min\"}";
  sendJson(200, json);
}

// GET /api/pressure —— 压力（0-1MPa）
void handlePressure() {
  // 即使读数无效也带出原始值，方便标定和排查接线
  String json = "{\"value\":" + String(pressureOk ? String(lastPressure, 3) : String("null")) +
                ",\"unit\":\"MPa\"," +
                "\"ok\":" + String(pressureOk ? "true" : "false") + "," +
                "\"raw\":" + String(lastPressureRaw, 0) + "," +
                "\"pinVoltage\":" + String(lastPressurePinVoltage, 3) + "," +
                "\"voltage\":" + String(lastPressureVoltage, 3) + "}";
  sendJson(pressureOk ? 200 : 503, json);
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

// GET /api/heater/on —— 开加热
void handleHeaterOn() {
  setHeater(true);
  sendJson(200, "{\"status\":\"ok\",\"heater\":true}");
}

// GET /api/heater/off —— 关加热
void handleHeaterOff() {
  setHeater(false);
  sendJson(200, "{\"status\":\"ok\",\"heater\":false}");
}

// GET /api/heater/toggle —— 切换加热开关
void handleHeaterToggle() {
  setHeater(!heaterState);
  String json = "{\"status\":\"ok\",\"heater\":" + String(heaterState ? "true" : "false") + "}";
  sendJson(200, json);
}

// GET /api/heater/state —— 查询加热状态
void handleHeaterState() {
  String json = "{\"heater\":" + String(heaterState ? "true" : "false") + "}";
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
  json += "\"temperature\":" + String(tempOk ? String(lastWaterTemp, 1) : String("null")) + ",";
  json += "\"tempOk\":" + String(tempOk ? "true" : "false") + ",";
  json += "\"temperature2\":" + String(tempOk2 ? String(lastWaterTemp2, 1) : String("null")) + ",";
  json += "\"tempOk2\":" + String(tempOk2 ? "true" : "false") + ",";
  json += "\"pressure\":" + String(pressureOk ? String(lastPressure, 3) : String("null")) + ",";
  json += "\"pressureOk\":" + String(pressureOk ? "true" : "false") + ",";
  json += "\"level\":" + String(levelOk ? String(lastLevelPercent, 1) : String("null")) + ",";
  json += "\"levelOk\":" + String(levelOk ? "true" : "false") + ",";
  json += "\"levelMm\":" + String(levelOk ? String(lastLevelMm, 1) : String("null")) + ",";
  json += "\"distanceMm\":" + String(distanceOk ? String(lastDistanceMm, 1) : String("null")) + ",";
  json += "\"tankHeightMm\":" + String(tankHeightMm, 1) + ",";
  json += "\"light\":" + String(lightOk ? String(lastLux, 1) : String("null")) + ",";
  json += "\"lightOk\":" + String(lightOk ? "true" : "false") + ",";
  json += "\"pump\":" + String(pumpState ? "true" : "false") + ",";
  json += "\"pumpTarget\":" + String(pumpTargetLiters, 2) + ",";
  json += "\"heater\":" + String(heaterState ? "true" : "false");
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
  server.on("/api/temperature", handleTemperature);
  server.on("/api/temperature2", handleTemperature2);
  server.on("/api/pressure", handlePressure);
  server.on("/api/level", handleLevel);
  server.on("/api/level/height", handleLevelHeight);
  server.on("/api/light", handleLight);
  server.on("/api/pump/on", handlePumpOn);
  server.on("/api/pump/off", handlePumpOff);
  server.on("/api/pump/toggle", handlePumpToggle);
  server.on("/api/pump/state", handlePumpState);
  server.on("/api/pump/target", handlePumpTarget);
  server.on("/api/heater/on", handleHeaterOn);
  server.on("/api/heater/off", handleHeaterOff);
  server.on("/api/heater/toggle", handleHeaterToggle);
  server.on("/api/heater/state", handleHeaterState);
  server.on("/api/health", handleHealth);
  server.onNotFound(handleNotFound);
}
