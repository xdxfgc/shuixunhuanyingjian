// ============================================================
// 第二路加热继电器模块（D16）
// 负责：初始化（上电默认关）和按触发极性开关第二个加热模块
//
// 接线：DC+ -> 5V（或模块额定电压），DC- -> GND（必须和 ESP32 共地），IN -> D16
//       加热模块电源独立，接继电器 COM/NO，不要从 ESP32 取电
//       ⚠️ 跳线帽极性和 HEATER2_ACTIVE_HIGH 必须一致（H 对应 1，L 对应 0）
// ============================================================
#include "config.h"

void initHeaterRelay2() {
  pinMode(HEATER2_PIN, OUTPUT);
  setHeater2(false);        // 上电默认关闭加热
  Serial.println("加热继电器 #2 初始化完成（D16，上电默认关闭）");
}

// 控制第二个加热模块（按 HEATER2_ACTIVE_HIGH 极性）
void setHeater2(bool on) {
  heaterState2 = on;
#if HEATER2_ACTIVE_HIGH
  digitalWrite(HEATER2_PIN, on ? HIGH : LOW);
#else
  digitalWrite(HEATER2_PIN, on ? LOW : HIGH);
#endif
  Serial.printf("加热2%s\n", on ? "已开启" : "已关闭");
}
