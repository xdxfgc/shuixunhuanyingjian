// ============================================================
// DS18B20 防水温度传感器模块 #2（D25）
// 负责：非阻塞读取第二路水温，每 2 秒更新缓存 lastWaterTemp2
// 接线：红 -> 3.3V，黑 -> GND，黄(信号) -> D25 + 4.7kΩ 上拉到 3.3V
// 依赖库：OneWire、DallasTemperature（Arduino 库管理器搜索安装）
// ============================================================
#include "config.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// 模块内部对象与状态（不对外暴露）
static OneWire oneWire2(TEMP2_PIN);
static DallasTemperature tempSensors2(&oneWire2);
static bool sensorFound2 = false;       // 是否检测到传感器
static bool conversionPending2 = false; // 是否正在等转换完成
static unsigned long lastRequestMs2 = 0;

void initTempSensor2() {
  // 兜底：开 ESP32 内部弱上拉（约 45kΩ）。正常应外接 4.7kΩ，效果更好
  pinMode(TEMP2_PIN, INPUT_PULLUP);
  tempSensors2.begin();
  sensorFound2 = (tempSensors2.getDeviceCount() > 0);
  tempSensors2.setWaitForConversion(false); // 非阻塞：不在这里等 750ms

  lastWaterTemp2 = NAN;
  tempOk2 = false;

  if (sensorFound2) {
    Serial.printf("DS18B20 #2 温度传感器初始化完成（D25，发现 %d 个）\n",
                  tempSensors2.getDeviceCount());
    tempSensors2.requestTemperatures();
    lastRequestMs2 = millis();
    conversionPending2 = true;
  } else {
    Serial.println("DS18B20 #2 未检测到：检查 D25 接线和 4.7kΩ 上拉电阻");
  }
}

// 非阻塞轮询：主循环每圈调用一次，不阻塞 HTTP 服务器
void processTempSensor2() {
  if (!sensorFound2) return;

  unsigned long now = millis();

  if (!conversionPending2) {
    if (now - lastRequestMs2 >= TEMP2_READ_INTERVAL_MS) {
      tempSensors2.requestTemperatures();
      lastRequestMs2 = now;
      conversionPending2 = true;
    }
    return;
  }

  if (now - lastRequestMs2 >= TEMP2_CONVERSION_MS) {
    conversionPending2 = false;
    float t = tempSensors2.getTempCByIndex(0);
    if (t > -55.0 && t < 125.0 && t != 85.0) {
      lastWaterTemp2 = t;
      tempOk2 = true;
    } else {
      tempOk2 = false;
      Serial.printf("DS18B20 #2 读数异常(%.1f℃)：请检查接线与上拉电阻\n", t);
    }
  }
}
