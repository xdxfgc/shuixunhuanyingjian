// ============================================================
// 继电器 / 水泵控制模块（WKY-1-RELAY-1）
// 负责：初始化（上电默认关）和按触发极性开关水泵
// ============================================================
#include "config.h"

void initRelay() {
  pinMode(RELAY_PIN, OUTPUT);
  setPump(false);
  Serial.println("继电器水泵初始化完成（D32，上电默认关闭）");
}

// 控制水泵开关（按 RELAY_ACTIVE_HIGH 极性）
void setPump(bool on) {
  pumpState = on;
#if RELAY_ACTIVE_HIGH
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
#else
  digitalWrite(RELAY_PIN, on ? LOW : HIGH);
#endif
  Serial.printf("水泵%s\n", on ? "已开启" : "已关闭");
}
