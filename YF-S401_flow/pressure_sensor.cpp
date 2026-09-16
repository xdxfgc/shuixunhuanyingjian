// ============================================================
// 压力传感器模块（0-1MPa 模拟输出，D33）
// 负责：非阻塞读取压力，每秒更新缓存 lastPressure
//
// 接线：
//   红 -> 5V（外部电源，必须和 ESP32 共地）
//   黑 -> GND
//   黄 -> 10kΩ 串到 D33，D33 再接 20kΩ 到 GND（分压，防止 4.5V 超量程）
//
// 换算：0.5V -> 0MPa，4.5V -> 1MPa（若你的型号不是 0.5-4.5V，改 config.h 里的宏）
// ============================================================
#include "config.h"

static unsigned long lastPressureMs = 0;
static bool warnedOverRange = false;  // ADC 满量程只提示一次，避免刷屏

void initPressureSensor() {
  pinMode(PRESSURE_PIN, INPUT);
  analogReadResolution(12);  // 0~4095
  // 明确设置 11dB 衰减（量程约 0~3.3V），不依赖默认值
  analogSetPinAttenuation(PRESSURE_PIN, ADC_11db);
  lastPressure = 0.0;
  lastPressureVoltage = 0.0;
  lastPressureRaw = 0.0;
  lastPressurePinVoltage = 0.0;
  pressureOk = false;
  Serial.println("压力传感器初始化完成（D33，0-1MPa 模拟输出）");
}

// 非阻塞轮询：主循环每圈调用一次
void processPressureSensor() {
  unsigned long now = millis();
  if (now - lastPressureMs < PRESSURE_SAMPLE_INTERVAL_MS) return;
  lastPressureMs = now;

  // 多次采样取平均，降低 ADC 噪声
  uint32_t sum = 0;
  for (int i = 0; i < PRESSURE_SAMPLES; i++) {
    sum += analogRead(PRESSURE_PIN);
  }
  float raw = sum / (float)PRESSURE_SAMPLES;

  // 引脚电压 -> 还原成传感器实际输出电压（去掉分压）
  float vPin = raw / 4095.0 * PRESSURE_VREF;
  float vSig = vPin / PRESSURE_DIVIDER_RATIO;

  // 原始值始终记录，方便标定和排查（即使本次读数无效）
  lastPressureRaw = raw;
  lastPressurePinVoltage = vPin;
  lastPressureVoltage = vSig;

  // ADC 顶到满量程：D33 电压 >= ~3.3V，多半是分压电阻没接，或传感器输出超范围
  if (raw >= 4090.0) {
    pressureOk = false;
    if (!warnedOverRange) {
      Serial.println("压力 ADC 满量程：D33 电压过高，检查 10kΩ+20kΩ 分压电阻是否接好");
      warnedOverRange = true;
    }
    return;
  }

  // 合理性判断：太低（断线/未接）或太高视为无效
  if (vSig < 0.3 || vSig > 5.3) {
    pressureOk = false;
    return;
  }

  float p = (vSig - PRESSURE_V_MIN) / (PRESSURE_V_MAX - PRESSURE_V_MIN) * PRESSURE_P_MAX;
  if (p < 0) p = 0;
  if (p > PRESSURE_P_MAX) p = PRESSURE_P_MAX;

  lastPressure = p;
  pressureOk = true;
}
