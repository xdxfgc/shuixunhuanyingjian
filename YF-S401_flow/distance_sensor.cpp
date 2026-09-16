// ============================================================
// 超声波测距模块（HC-SR04 / JSN-SR04T，Trig + Echo）→ 用来算水位
// 负责：每 0.5 秒测一次「传感器 → 水面」的距离，再换算成水位
//
//   水位 = 预定高度(TANK_HEIGHT_MM) − 测距值
//   箱子越空 → 测距越大 → 水位越低；水越满 → 测距越小 → 水位越高
//
// 接线：VCC -> 5V，GND -> GND，Trig -> D23，Echo -> 分压 -> D18
//   ⚠️ Echo 输出 5V，必须分压：Echo →1kΩ→ D18，D18 →2kΩ→ GND（约 3.3V）
//      3.3V 版本模块（RCWL-1601 等）可直连
//
// 测距原理：Trig 给 10µs 高电平触发，Echo 回一个高电平脉冲，
//   脉宽(µs) × 0.343 / 2 = 距离(mm)（声速按 343m/s 算，约 20℃ 空气）
//
// 预定高度（TANK_HEIGHT_MM）：传感器探头面到箱底的距离，单位毫米。
//   量法：箱子排空后，量探头面到箱底内壁的垂直距离，填进 config.h；
//   也可运行时用 GET /api/level/height?value=250 改（写 NVS，掉电不丢）。
// ============================================================
#include "config.h"
#include <Preferences.h>

static unsigned long lastDistanceMs = 0;
static bool warnedNoEcho = false;      // 没收到回波只提示一次
static Preferences distancePrefs;      // 存预定高度（命名空间 tankcfg）

// 最近几次有效读数，取中位数抗干扰（水波、泡沫会让个别读数跳变）
static float medianBuf[ULTRASONIC_MEDIAN_SAMPLES];
static uint8_t medianCount = 0;
static uint8_t medianIndex = 0;

// 把预定高度写进 NVS
static void saveTankHeight() {
  if (distancePrefs.begin("tankcfg", false)) {
    distancePrefs.putFloat("tank", tankHeightMm);
    distancePrefs.end();
  } else {
    Serial.println("预定高度写入 NVS 失败，本次设置仅本次运行有效");
  }
}

// 取中位数（样本很少，直接插入排序）
static float medianValue() {
  if (medianCount == 0) return 0.0;

  float tmp[ULTRASONIC_MEDIAN_SAMPLES];
  for (uint8_t i = 0; i < medianCount; i++) tmp[i] = medianBuf[i];
  for (uint8_t i = 1; i < medianCount; i++) {
    float key = tmp[i];
    int8_t j = i - 1;
    while (j >= 0 && tmp[j] > key) {
      tmp[j + 1] = tmp[j];
      j--;
    }
    tmp[j + 1] = key;
  }
  return tmp[medianCount / 2];
}

// 等待回波的超时时间：按预定高度 + 余量算，量程外就不用干等
static uint32_t echoTimeoutUs() {
  float mm = tankHeightMm + ULTRASONIC_EXTRA_TIMEOUT_MM;
  float us = mm * 2.0 / 0.343;   // 往返
  if (us < 3000.0) us = 3000.0;
  if (us > 30000.0) us = 30000.0;
  return (uint32_t)us;
}

void initDistanceSensor() {
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);   // 平时拉低，避免误触发
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);

  tankHeightMm = TANK_HEIGHT_MM;

  // 有历史设置就用 NVS 里的值覆盖默认宏
  if (distancePrefs.begin("tankcfg", true)) {
    float saved = distancePrefs.getFloat("tank", tankHeightMm);
    distancePrefs.end();
    if (saved > 0.0) tankHeightMm = saved;
  }

  lastDistanceMm = 0.0;
  lastEchoUs = 0.0;
  distanceOk = false;
  lastLevelMm = 0.0;
  lastLevelPercent = 0.0;
  levelOk = false;
  medianCount = 0;
  medianIndex = 0;

  Serial.printf("超声波测距初始化完成（Trig D%d / Echo D%d，预定高度 %.1fmm）\n",
                ULTRASONIC_TRIG_PIN, ULTRASONIC_ECHO_PIN, tankHeightMm);
  Serial.println("水位 = 预定高度 − 测距值；预定高度可用 /api/level/height?value=xxx 修改");
}

// 非阻塞轮询：主循环每圈调用一次
void processDistanceSensor() {
  unsigned long now = millis();
  if (now - lastDistanceMs < ULTRASONIC_SAMPLE_INTERVAL_MS) return;
  lastDistanceMs = now;

  // ---- 触发一次测量 ----
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(4);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);                    // 至少 10µs 高电平
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  // ---- 量回波高电平宽度（超时时间按量程算，所以最多阻塞几毫秒）----
  unsigned long us = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, echoTimeoutUs());
  lastEchoUs = (float)us;

  if (us == 0) {
    // 量程内没收到回波：水面太远、被泡沫/水雾挡住，或者 Echo 没接好
    distanceOk = false;
    levelOk = false;
    if (!warnedNoEcho) {
      Serial.println("超声波没有收到回波：检查 Echo 分压接线（Echo→1k→D18，D18→2k→GND）、"
                     "传感器朝向，以及预定高度是否设得比实际距离小");
      warnedNoEcho = true;
    }
    return;
  }

  // ---- 脉宽换成距离 ----
  float distanceMm = us * 0.343 / 2.0;
  if (distanceMm < 20.0 || distanceMm > 5000.0) {
    // HC-SR04 盲区约 2cm；超过 5m 视为不可信
    distanceOk = false;
    levelOk = false;
    return;
  }

  // ---- 中位数滤波 ----
  medianBuf[medianIndex] = distanceMm;
  medianIndex = (medianIndex + 1) % ULTRASONIC_MEDIAN_SAMPLES;
  if (medianCount < ULTRASONIC_MEDIAN_SAMPLES) medianCount++;

  float filtered = medianValue();
  lastDistanceMm = filtered;
  distanceOk = true;

  // ---- 水位 = 预定高度 − 测距值 ----
  float levelMm = tankHeightMm - filtered;
  if (levelMm < 0.0) levelMm = 0.0;                 // 测到的比箱底还远，按空箱算
  if (levelMm > tankHeightMm) levelMm = tankHeightMm;

  lastLevelMm = levelMm;
  lastLevelPercent = (tankHeightMm > 0.0) ? (levelMm / tankHeightMm * 100.0) : 0.0;
  levelOk = true;
}

// 修改预定高度（毫米）并写入 NVS（供 /api/level/height 调用）
void setTankHeightMm(float mm) {
  tankHeightMm = mm;
  saveTankHeight();
  Serial.printf("预定高度已更新：%.1fmm（水位 = 该值 − 测距值）\n", tankHeightMm);
}
