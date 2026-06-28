// SensorManager.h - SCD41 sensor abstraction
// Adapted from CrowPanel SCD41 project for M5Stack (Wire1 / RTC-based timing)
//
// June 2026 - Tetsu Nishimura
#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <M5Unified.h>

// ============================================================
// テスト用フラグ
// コメントアウトを外すとダミーデータを使用する (UIテスト用)
// ============================================================
// #define USE_DUMMY_SENSOR

namespace SensorManager {

    // センサー初期化 (Wire1 / SCD41)
    void init();

    // センサーデータ取得
    //  1 : 取得成功 (co2, temperature, humidity にセット)
    //  0 : データ未準備 (前回値を保持)
    // -1 : 取得エラー
    int readData(uint16_t &co2, float &temperature, float &humidity);

    // ウォームアップ中か (起動後180秒)
    bool isWarmingUp();

    // 1分の集計タイミングか (RTCの分変化で判定)
    bool isAggregationTime(m5::rtc_datetime_t &rtcData);

    // デバッグ用
    uint16_t getLastError();
    uint16_t getAscStatus();

} // namespace SensorManager

#endif // SENSOR_MANAGER_H
