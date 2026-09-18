// ============================================================
// 第二路继电器 / 水泵控制模块（D14）
// 负责：初始化（上电默认关）和按触发极性开关第二个水泵
//
// 接线：DC+ -> 5V（或模块额定电压），DC- -> GND（必须和 ESP32 共地），IN -> D14
//       水泵电源独立，接继电器 COM/NO，不要从 ESP32 取电
//       ⚠️ 跳线帽极性和 RELAY2_ACTIVE_HIGH 必须一致（H 对应 1，L 对应 0）
// ============================================================
#include "config.h"

void initRelay2() {
  pinMode(RELAY2_PIN, OUTPUT);
  setPump2(false);          // 上电默认关闭
  Serial.println("继电器水泵 #2 初始化完成（D14，上电默认关闭）");
}

// 控制第二个水泵开关（按 RELAY2_ACTIVE_HIGH 极性）
void setPump2(bool on) {
  pumpState2 = on;
#if RELAY2_ACTIVE_HIGH
  digitalWrite(RELAY2_PIN, on ? HIGH : LOW);
#else
  digitalWrite(RELAY2_PIN, on ? LOW : HIGH);
#endif
  Serial.printf("水泵2%s\n", on ? "已开启" : "已关闭");
}
