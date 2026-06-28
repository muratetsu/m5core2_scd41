// SensorManager.cpp - SCD41 sensor abstraction
// M5Stack Core2: Wire1 (Port A: GPIO32/33), RTC-based aggregation timing
//
// June 2026 - Tetsu Nishimura

#include "SensorManager.h"
#include "Logger.h"
#include <Wire.h>

#ifndef USE_DUMMY_SENSOR
#include <SensirionI2cScd4x.h>
static SensirionI2cScd4x scd4x;
#endif

namespace SensorManager {

#ifdef USE_DUMMY_SENSOR
    static const uint32_t SENSOR_WARMUP_MS = 10000; // テスト用: 10秒
    static uint16_t last_error_code = 0;
    static uint16_t cached_asc = 1;
    static uint32_t lastRead = 0;
#else
    static const uint32_t SENSOR_WARMUP_MS = 180000; // 本番: 3分
    static uint16_t last_error_code = 0;
    static uint16_t cached_asc = 0;
    static uint32_t lastRead = 0;
#endif

// 前回のRTC分 (isAggregationTime用)
static int prevMinute = -1;

// ============================================================
// ウォームアップ判定
// ============================================================
bool isWarmingUp() {
    return millis() < SENSOR_WARMUP_MS;
}

// ============================================================
// センサー初期化
// ============================================================
void init() {
#ifdef USE_DUMMY_SENSOR
    LOG_I("SensorManager", "Dummy mode: SCD41 hardware init skipped.");
#else
    // M5Stack Core2 Port A: GPIO32(SDA), GPIO33(SCL)
    Wire1.begin(32, 33);
    scd4x.begin(Wire1, SCD41_I2C_ADDR_62);

    uint16_t err;
    err = scd4x.wakeUp();
    if (err) { LOG_E("SCD41", "wakeUp error: %d", err); }

    err = scd4x.stopPeriodicMeasurement();
    if (err) { LOG_E("SCD41", "stop error: %d", err); }

    err = scd4x.reinit();
    if (err) { LOG_E("SCD41", "reinit error: %d", err); }

    // ASC (自動キャリブレーション) ステータスを読み取る
    scd4x.getAutomaticSelfCalibrationEnabled(cached_asc);

    uint64_t serial;
    if (!scd4x.getSerialNumber(serial)) {
        LOG_I("SCD41", "Serial: 0x%04x%08x",
              (uint32_t)(serial >> 32), (uint32_t)(serial & 0xFFFFFFFF));
    }

    err = scd4x.startPeriodicMeasurement();
    if (err) { LOG_E("SCD41", "start error: %d", err); }

    LOG_I("SensorManager", "SCD41 initialized (Wire1 / Port A).");
#endif
}

// ============================================================
// センサーデータ取得
// ============================================================
int readData(uint16_t &co2, float &temperature, float &humidity) {
#ifdef USE_DUMMY_SENSOR
    if (millis() - lastRead < 1000) return 0;
    static float phase = 0.0f;
    co2         = (uint16_t)(1200 + 800 * sinf(phase));
    temperature = 25.0f + (random(-10, 10) / 10.0f);
    humidity    = 50.0f + (random(-50, 50) / 10.0f);
    phase += 0.1f;
    lastRead = millis();
    return 1;

#else
    // 5秒ポーリング
    if (millis() - lastRead < 5000) return 0;

    bool isDataReady = false;
    uint16_t error = scd4x.getDataReadyStatus(isDataReady);
    if (error) {
        last_error_code = error;
        return 0;
    }
    if (!isDataReady) return 0;

    error = scd4x.readMeasurement(co2, temperature, humidity);
    if (error) {
        last_error_code = error;
        return -1;
    }
    if (co2 == 0) return -1;

    last_error_code = 0;
    lastRead = millis();
    return 1;
#endif
}

// ============================================================
// 集計タイミング判定 (RTC分変化ベース)
// NTP未取得でもRTCが動作していれば機能する
// ============================================================
bool isAggregationTime(m5::rtc_datetime_t &rtcData) {
#ifdef USE_DUMMY_SENSOR
    static uint32_t lastAggTime = 0;
    if (millis() - lastAggTime >= 1000) {
        lastAggTime = millis();
        return true;
    }
    return false;
#else
    if (prevMinute == -1) {
        prevMinute = rtcData.time.minutes;
        return false;
    }
    bool changed = (prevMinute != rtcData.time.minutes);
    prevMinute = rtcData.time.minutes;
    return changed;
#endif
}

// ============================================================
// デバッグ用
// ============================================================
uint16_t getLastError() { return last_error_code; }
uint16_t getAscStatus() { return cached_asc; }

} // namespace SensorManager
