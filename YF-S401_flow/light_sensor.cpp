// ============================================================
// GY-302 光照传感器模块（BH1750 芯片，I2C）
// 负责：非阻塞读取环境光照强度（lx），每秒更新缓存 lastLux
//
// 接线：VCC -> 3.3V，GND -> GND，SDA -> D21，SCL -> D22
//       GY-302 模块板载 4.7kΩ 上拉，无需外接电阻；ADDR 悬空即 0x23
//
// 不依赖第三方库：直接按 BH1750 时序用 Wire 发指令、读回 2 字节原始值
//   原始值 / 1.2 = 光照强度 lx（高分辨率连续模式，分辨率约 1 lx）
// ============================================================
#include "config.h"

#define BH1750_POWER_ON 0x01      // 上电（上电默认是掉电模式，必须先发这条）
#define BH1750_CONT_H_RES 0x10    // 连续高分辨率模式，1 lx，约 120ms 一次

static uint8_t lightAddr = 0;         // 实际探测到的 I2C 地址
static bool lightFound = false;       // 是否检测到传感器
static unsigned long lastLightMs = 0;

// 发一条单字节指令，返回是否成功
static bool lightWrite(uint8_t cmd) {
  Wire.beginTransmission(lightAddr);
  Wire.write(cmd);
  return Wire.endTransmission() == 0;
}

// 探测某个地址上是否有器件应答
static bool lightProbe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void initLightSensor() {
  Wire.begin(LIGHT_SDA_PIN, LIGHT_SCL_PIN);
  Wire.setClock(100000);  // BH1750 标准模式 100kHz，稳定优先

  lastLux = 0.0;
  lightOk = false;
  lightFound = false;

  if (lightProbe(LIGHT_ADDR_PRIMARY)) {
    lightAddr = LIGHT_ADDR_PRIMARY;
  } else if (lightProbe(LIGHT_ADDR_ALT)) {
    lightAddr = LIGHT_ADDR_ALT;
  } else {
    Serial.printf("GY-302 未检测到：检查 SDA->D%d / SCL->D%d 接线和 3.3V 供电\n",
                  LIGHT_SDA_PIN, LIGHT_SCL_PIN);
    return;
  }

  lightWrite(BH1750_POWER_ON);
  delay(10);                      // 等芯片进入上电状态
  lightWrite(BH1750_CONT_H_RES);
  lightFound = true;
  lastLightMs = millis();         // 第一帧转换约 120ms，1 秒后再读

  Serial.printf("GY-302 光照传感器初始化完成（I2C 0x%02X，SDA D%d / SCL D%d）\n",
                lightAddr, LIGHT_SDA_PIN, LIGHT_SCL_PIN);
}

// 非阻塞轮询：主循环每圈调用一次，不阻塞 HTTP 服务器
void processLightSensor() {
  if (!lightFound) return;

  unsigned long now = millis();
  if (now - lastLightMs < LIGHT_READ_INTERVAL_MS) return;
  lastLightMs = now;

  // 连续高分辨率模式下直接读回上一次转换结果（2 字节，大端）
  Wire.requestFrom((uint8_t)lightAddr, (uint8_t)2);
  if (Wire.available() < 2) {
    lightOk = false;
    return;
  }

  uint16_t raw = ((uint16_t)Wire.read() << 8) | (uint16_t)Wire.read();
  lastLux = raw / 1.2;
  lightOk = true;
}
