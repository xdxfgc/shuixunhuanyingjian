// ============================================================
// 第二路超声波测距模块（HC-SR04 / JSN-SR04T）→ 水位2
// 接线：VCC -> 5V，GND -> GND，Trig -> D4，Echo -> 1kΩ -> D13 -> 2kΩ -> GND
//
//   水位2 = 预定高度2(TANK_HEIGHT_MM2) − 测距值
//   预定高度可用 GET /api/level2/height?value=100 修改（写 NVS，掉电不丢）
// ============================================================
#include "config.h"
#include <Preferences.h>

static unsigned long lastDistanceMs2 = 0;
static bool warnedNoEcho2 = false;          // 没收到回波只提示一次
static Preferences distancePrefs2;          // 存预定高度（命名空间 tankcfg2，和第一路分开）

static float medianBuf2[ULTRASONIC_MEDIAN_SAMPLES];
static uint8_t medianCount2 = 0;
static uint8_t medianIndex2 = 0;

// 把预定高度写进 NVS
static void saveTankHeight2() {
  if (distancePrefs2.begin("tankcfg2", false)) {
    distancePrefs2.putFloat("tank", tankHeightMm2);
    distancePrefs2.end();
  } else {
    Serial.println("水位2 预定高度写入 NVS 失败，本次设置仅本次运行有效");
  }
}

// 取中位数（样本很少，直接插入排序）
static float medianValue2() {
  if (medianCount2 == 0) return 0.0;

  float tmp[ULTRASONIC_MEDIAN_SAMPLES];
  for (uint8_t i = 0; i < medianCount2; i++) tmp[i] = medianBuf2[i];
  for (uint8_t i = 1; i < medianCount2; i++) {
    float key = tmp[i];
    int8_t j = i - 1;
    while (j >= 0 && tmp[j] > key) {
      tmp[j + 1] = tmp[j];
      j--;
    }
    tmp[j + 1] = key;
  }
  return tmp[medianCount2 / 2];
}

// 等待回波的超时时间：按预定高度 + 余量算
static uint32_t echoTimeoutUs2() {
  float mm = tankHeightMm2 + ULTRASONIC_EXTRA_TIMEOUT_MM;
  float us = mm * 2.0 / 0.343;   // 往返
  if (us < 3000.0) us = 3000.0;
  if (us > 30000.0) us = 30000.0;
  return (uint32_t)us;
}

void initDistanceSensor2() {
  pinMode(ULTRASONIC2_TRIG_PIN, OUTPUT);
  digitalWrite(ULTRASONIC2_TRIG_PIN, LOW);   // 平时拉低，避免误触发
  pinMode(ULTRASONIC2_ECHO_PIN, INPUT);

  tankHeightMm2 = TANK_HEIGHT_MM2;

  // 有历史设置就用 NVS 里的值覆盖默认宏
  if (distancePrefs2.begin("tankcfg2", true)) {
    float saved = distancePrefs2.getFloat("tank", tankHeightMm2);
    distancePrefs2.end();
    if (saved > 0.0) tankHeightMm2 = saved;
  }

  lastDistanceMm2 = 0.0;
  lastEchoUs2 = 0.0;
  distanceOk2 = false;
  lastLevelMm2 = 0.0;
  lastLevelPercent2 = 0.0;
  levelOk2 = false;
  medianCount2 = 0;
  medianIndex2 = 0;

  Serial.printf("超声波 #2 初始化完成（Trig D%d / Echo D%d，预定高度 %.1fmm）\n",
                ULTRASONIC2_TRIG_PIN, ULTRASONIC2_ECHO_PIN, tankHeightMm2);
}

// 非阻塞轮询：主循环每圈调用一次
void processDistanceSensor2() {
  unsigned long now = millis();
  if (now - lastDistanceMs2 < ULTRASONIC_SAMPLE_INTERVAL_MS) return;
  lastDistanceMs2 = now;

  // ---- 触发一次测量 ----
  digitalWrite(ULTRASONIC2_TRIG_PIN, LOW);
  delayMicroseconds(4);
  digitalWrite(ULTRASONIC2_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC2_TRIG_PIN, LOW);

  // ---- 量回波高电平宽度 ----
  unsigned long us = pulseIn(ULTRASONIC2_ECHO_PIN, HIGH, echoTimeoutUs2());
  lastEchoUs2 = (float)us;

  if (us == 0) {
    distanceOk2 = false;
    levelOk2 = false;
    if (!warnedNoEcho2) {
      Serial.println("超声波 #2 没有收到回波：检查 Echo 分压接线（Echo→1k→D13，D13→2k→GND）、"
                     "传感器朝向，以及预定高度是否设得比实际距离小");
      warnedNoEcho2 = true;
    }
    return;
  }

  float distanceMm = us * 0.343 / 2.0;
  if (distanceMm < 20.0 || distanceMm > 5000.0) {
    distanceOk2 = false;
    levelOk2 = false;
    return;
  }

  // ---- 中位数滤波 ----
  medianBuf2[medianIndex2] = distanceMm;
  medianIndex2 = (medianIndex2 + 1) % ULTRASONIC_MEDIAN_SAMPLES;
  if (medianCount2 < ULTRASONIC_MEDIAN_SAMPLES) medianCount2++;

  float filtered = medianValue2();
  lastDistanceMm2 = filtered;
  distanceOk2 = true;

  // ---- 水位 = 预定高度 − 测距值 ----
  float levelMm = tankHeightMm2 - filtered;
  if (levelMm < 0.0) levelMm = 0.0;
  if (levelMm > tankHeightMm2) levelMm = tankHeightMm2;

  lastLevelMm2 = levelMm;
  lastLevelPercent2 = (tankHeightMm2 > 0.0) ? (levelMm / tankHeightMm2 * 100.0) : 0.0;
  levelOk2 = true;
}

// 修改预定高度（毫米）并写入 NVS（供 /api/level2/height 调用）
void setTankHeightMm2(float mm) {
  tankHeightMm2 = mm;
  saveTankHeight2();
  Serial.printf("水位2 预定高度已更新：%.1fmm（水位 = 该值 − 测距值）\n", tankHeightMm2);
}
