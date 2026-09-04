// ============================================================
// 流量传感器模块（YF-S401）
// 负责：中断计数、每 1 秒结算瞬时流量、累计水量、开机清零
// ============================================================
#include "config.h"

// 中断回调：只计数器，越短越好
static void IRAM_ATTR pulseISR() {
  pulseCount++;
}

void initFlowSensor() {
  pinMode(FLOW_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseISR, RISING);
  Serial.println("YF-S401 流量传感器初始化完成（D34，分压接入，上升沿计数）");
  lastPulseCount = readPulseCount();
  lastSampleMs = millis();
}

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
