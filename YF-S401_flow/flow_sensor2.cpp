// ============================================================
// 第二路流量传感器模块（YF-S401 #2）
// 负责：中断计数、每 1 秒结算瞬时流量、累计水量、开机清零
// 接线：红->5V  黑->GND  黄(信号)->10kΩ 串到 D19，D19 再接 20kΩ 到 GND
// ============================================================
#include "config.h"

// 中断回调：只计数器，越短越好
static void IRAM_ATTR pulseISR2() {
  pulseCount2++;
}

void initFlowSensor2() {
  pinMode(FLOW2_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(FLOW2_PIN), pulseISR2, RISING);
  Serial.println("YF-S401 #2 流量传感器初始化完成（D19，分压接入，上升沿计数）");
  lastPulseCount2 = readPulseCount2();
  lastSampleMs2 = millis();
}

// 读取当前脉冲计数（关中断再读，避免读到一半被 ISR 打断）
unsigned long readPulseCount2() {
  noInterrupts();
  unsigned long val = pulseCount2;
  interrupts();
  return val;
}

// 结算一次：根据窗口内脉冲数换算瞬时流量和累计量
void sampleFlow2() {
  unsigned long now = millis();
  unsigned long cnt = readPulseCount2();

  // 窗口用时（避免时钟回跳 / 首次为 0）
  unsigned long windowMs = (now >= lastSampleMs2) ? (now - lastSampleMs2) : 0;
  unsigned long delta = cnt - lastPulseCount2;

  if (windowMs > 0) {
    float windowSec = windowMs / 1000.0;
    // 瞬时流量(L/min) = 窗口内脉冲数 / (系数 × 窗口秒数)
    lastFlowRate2 = delta / (PULSES_PER_LITER2 * windowSec);
    // 累计水量 += 瞬时流量 × 窗口分钟数
    totalLiters2 += lastFlowRate2 * (windowSec / 60.0);
  } else {
    lastFlowRate2 = 0.0;
  }

  lastWindowPulses2 = delta;
  lastWindowMs2 = windowMs;
  lastPulseCount2 = cnt;
  lastSampleMs2 = now;

  // ★ 如果希望第二路也能触发「定量浇水自动关泵」，把下面这段的注释去掉：
  // if (pumpTargetLiters > 0 && totalLiters2 >= pumpTargetLiters) {
  //   if (pumpState) {
  //     setPump(false);
  //     Serial.println("第二路达到设定水量，自动关闭水泵");
  //   }
  // }
}

// 清空第二路累计过水总量
void resetTotal2() {
  noInterrupts();
  pulseCount2 = 0;
  interrupts();
  lastPulseCount2 = 0;
  lastSampleMs2 = millis();
  totalLiters2 = 0.0;
  lastFlowRate2 = 0.0;
  lastWindowPulses2 = 0;
  lastWindowMs2 = 0;
}
