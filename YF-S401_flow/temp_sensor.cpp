// ============================================================
// DS18B20 防水温度传感器模块（D27）
// 负责：非阻塞读取水温，每 2 秒更新缓存 lastWaterTemp
// 接线：红 -> 3.3V，黑 -> GND，黄(信号) -> D27 + 4.7kΩ 上拉到 3.3V
// 依赖库：OneWire、DallasTemperature（Arduino 库管理器搜索安装）
// ============================================================
#include "config.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// 模块内部对象与状态（不对外暴露）
static OneWire oneWire(TEMP_PIN);
static DallasTemperature tempSensors(&oneWire);
static bool sensorFound = false;       // 是否检测到传感器
static bool conversionPending = false; // 是否正在等转换完成
static unsigned long lastRequestMs = 0;

void initTempSensor() {
  // 兜底：开 ESP32 内部弱上拉（约 45kΩ）。正常应外接 4.7kΩ，效果更好
  pinMode(TEMP_PIN, INPUT_PULLUP);
  tempSensors.begin();
  sensorFound = (tempSensors.getDeviceCount() > 0);
  tempSensors.setWaitForConversion(false); // 非阻塞：不在这里等 750ms

  lastWaterTemp = NAN;
  tempOk = false;

  if (sensorFound) {
    Serial.printf("DS18B20 温度传感器初始化完成（D27，发现 %d 个）\n",
                  tempSensors.getDeviceCount());
    // 开机先发一次转换请求，随后由 processTempSensor 读回
    tempSensors.requestTemperatures();
    lastRequestMs = millis();
    conversionPending = true;
  } else {
    Serial.println("DS18B20 未检测到：检查 D27 接线和 4.7kΩ 上拉电阻");
  }
}

// 非阻塞轮询：主循环每圈调用一次，不阻塞 HTTP 服务器
void processTempSensor() {
  if (!sensorFound) return;

  unsigned long now = millis();

  if (!conversionPending) {
    // 到了间隔时间，发一次转换请求，立刻返回
    if (now - lastRequestMs >= TEMP_READ_INTERVAL_MS) {
      tempSensors.requestTemperatures();
      lastRequestMs = now;
      conversionPending = true;
    }
    return;
  }

  // 转换完成，读温度并更新缓存
  if (now - lastRequestMs >= TEMP_CONVERSION_MS) {
    conversionPending = false;
    float t = tempSensors.getTempCByIndex(0);
    // DS18B20 有效范围约 -55~125℃；85 是上电/错误特征值
    if (t > -55.0 && t < 125.0 && t != 85.0) {
      lastWaterTemp = t;
      tempOk = true;
    } else {
      tempOk = false;
      Serial.printf("DS18B20 读数异常(%.1f℃)：请检查接线与上拉电阻\n", t);
    }
  }
}
