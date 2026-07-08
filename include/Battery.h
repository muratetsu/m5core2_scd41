// Battery.h - Battery and power management for M5Stack Core2
//
// June 2026 - Tetsu Nishimura
#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include <M5Unified.h>

#define VBUS_THRESHOLD    2500  // VBUS接続の有無を判別する閾値(mV)
#define DISPLAY_OFF_TM    20    // バッテリー駆動時の画面消灯カウンタ (秒)

class Battery {
public:
    Battery();

    // 初期化 (バッテリーアイコンの表示位置を指定)
    void begin(int32_t posX, int32_t posY);

    // 電源状態を更新する (毎ループ呼ぶ)
    // 戻り値: LCD更新が必要な場合 true (USB再接続時など)
    bool updatePowerState();

    // バッテリー残量をグラフィックで表示する
    void showBatteryCapacity();

    // VBUSが接続されているか確認する
    bool isVbusConnected();

    // 画面消灯カウントダウンをリセットする
    void resetDisplayOffTimer();

    // LCDがON状態か
    bool lcdOn;

private:
    void wakeupLcd();
    void sleepLcd();

    int     _pwrMode;        // PWR_UNKNOWN / PWR_VBUS / PWR_BATTERY
    int     _displayOffCnt;  // 画面消灯カウントダウン
    int32_t _posX;
    int32_t _posY;
};

#endif // BATTERY_H
