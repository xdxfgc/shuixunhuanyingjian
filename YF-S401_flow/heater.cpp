// ============================================================
// 电磁继电器 / 加热模块（12V 加热模块）
// 负责：初始化和按触发极性开关加热
//
// 接线：
//   D26 -> 继电器模块信号线 IN
//   继电器线圈电源用独立电源（不要从 ESP32 取电！）
//   12V 加热模块 正/负 接继电器 COM/NO，随继电器吸合而通电
// ============================================================
#include "config.h"

void initHeaterRelay() {
  pinMode(HEATER_PIN, OUTPUT);
  setHeater(false);  // 上电默认关闭加热
  Serial.println("加热继电器初始化完成（D26，上电默认关闭）");
}

// 控制加热模块开关（按 HEATER_ACTIVE_HIGH 极性）
void setHeater(bool on) {
  heaterState = on;
#if HEATER_ACTIVE_HIGH
  digitalWrite(HEATER_PIN, on ? HIGH : LOW);
#else
  digitalWrite(HEATER_PIN, on ? LOW : HIGH);
#endif
  Serial.printf("加热%s\n", on ? "已开启" : "已关闭");
}
