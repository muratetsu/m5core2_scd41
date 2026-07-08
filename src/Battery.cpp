// Battery.cpp - Battery and power management for M5Stack Core2
//
// June 2026 - Tetsu Nishimura

#include "Battery.h"
#include "Logger.h"

// M5GFX用ローカル色定数 (RGB888)
static const uint32_t BAT_COLOR_OK  = 0x00cc00;
static const uint32_t BAT_COLOR_LOW = 0xff0000;

#define PWR_UNKNOWN 0
#define PWR_VBUS    1
#define PWR_BATTERY 2

Battery::Battery() {}

// ============================================================
// 初期化
// ============================================================
void Battery::begin(int32_t posX, int32_t posY) {
    _pwrMode      = PWR_UNKNOWN;
    lcdOn         = true;
    _displayOffCnt = DISPLAY_OFF_TM;
    _posX         = posX;
    _posY         = posY;

    // 外部出力ピンを無効化 (不要な電力消費を抑制)
    M5.Power.setExtOutput(false);
}

// ============================================================
// 電源状態更新 (loop()から毎ループ呼ぶ)
// 戻り値: LCD更新が必要なら true
// ============================================================
bool Battery::updatePowerState() {
    static bool toggle = false;
    bool lcdUpdateRequired = false;

    bool isCharging      = M5.Power.isCharging();
    bool isVbusConnected = this->isVbusConnected();

    if (isVbusConnected) {
        // USB接続時
        if (_pwrMode != PWR_VBUS) {
            _pwrMode = PWR_VBUS;
            if (!lcdOn) {
                wakeupLcd();
                lcdUpdateRequired = true;
            }
            M5.Power.setLed(true);
            _displayOffCnt = DISPLAY_OFF_TM;
        }
        // 充電中はLEDをトグル点滅
        if (isCharging) {
            toggle ^= true;
            M5.Power.setLed(toggle);
        }
    } else {
        // バッテリー駆動時
        if (_pwrMode != PWR_BATTERY) {
            _pwrMode = PWR_BATTERY;
            M5.Power.setLed(false);
        }
        static uint32_t lastDisplayOffTick = 0;
        if (millis() - lastDisplayOffTick >= 1000) {
            lastDisplayOffTick = millis();
            if (_displayOffCnt > 0) {
                _displayOffCnt--;
                // 消灯4秒前から輝度を下げて予告
                if (_displayOffCnt == 4) {
                    M5.Display.setBrightness(32);
                }
            } else {
                sleepLcd();
            }
        }
    }

    // Powerボタン押下で手動Sleep/Wake切り替え
    // M5.update() で PMIC レジスタがクリアされるため、M5.BtnPWR を使用
    if (M5.BtnPWR.wasClicked()) {
        LOG_D("Power", "Power button clicked! lcdOn=%d", lcdOn);
        if (lcdOn) {
            LOG_D("Power", "Going to sleep LCD");
            sleepLcd();
        } else {
            LOG_D("Power", "Waking up LCD");
            wakeupLcd();
            lcdUpdateRequired = true;
        }
    }

    // 画面消灯時のタッチによる復帰
    if (!lcdOn) {
        auto detail = M5.Touch.getDetail(0);
        if (detail.wasPressed()) {
            wakeupLcd();
            lcdUpdateRequired = true;
        }
    }

    return lcdUpdateRequired;
}

// ============================================================
// VBUS接続チェック
// ============================================================
bool Battery::isVbusConnected() {
    float vbus = M5.Power.getVBUSVoltage();
    bool isACIN = M5.Power.Axp192.isACIN();
    return (vbus > VBUS_THRESHOLD) || isACIN;
}

// ============================================================
// LCD ウェイクアップ
// ============================================================
void Battery::wakeupLcd() {
    _displayOffCnt = DISPLAY_OFF_TM;
    if (!lcdOn) {
        M5.Display.wakeup();
        M5.Display.setBrightness(128);
        lcdOn = true;
    }
}

// ============================================================
// LCD スリープ
// ============================================================
void Battery::sleepLcd() {
    if (lcdOn) {
        M5.Display.sleep();
        lcdOn = false;
    }
}

// ============================================================
// バッテリー残量グラフィック表示
// ============================================================
void Battery::showBatteryCapacity() {
    char buf[12];
    uint32_t color;
    int32_t capacity = M5.Power.getBatteryLevel();

    color = (capacity >= 20) ? BAT_COLOR_OK : BAT_COLOR_LOW;

    // バッテリー外形
    M5.Display.fillRect(_posX, _posY, 30, 16, TFT_BLACK);
    M5.Display.drawRoundRect(_posX, _posY + 1, 26, 14, 3, TFT_WHITE);
    M5.Display.drawLine(_posX + 26, _posY + 5, _posX + 26, _posY + 9, TFT_WHITE);
    M5.Display.drawLine(_posX + 27, _posY + 6, _posX + 27, _posY + 8, TFT_WHITE);

    // 残量バー
    M5.Display.fillRoundRect(_posX + 2, _posY + 3,
                              capacity * 22 / 100, 10, 2, color);

    // 充電中は稲妻マーク
    if (M5.Power.isCharging()) {
        M5.Display.fillTriangle(
            _posX + 14, _posY,
            _posX +  9, _posY + 8,
            _posX + 13, _posY + 8, TFT_YELLOW);
        M5.Display.fillTriangle(
            _posX + 12, _posY + 15,
            _posX + 17, _posY + 8,
            _posX + 13, _posY + 8, TFT_YELLOW);
    }

    // ％テキスト
    sprintf(buf, " %d%%", capacity);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawRightString(buf, _posX - 2, _posY, &fonts::Font2);
}
