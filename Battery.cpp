// Battery Management
//
// November 1, 2025
// Tetsu Nishimura

#include <Preferences.h>
#include "Battery.h"

#define PWR_UNKNOWN 0
#define PWR_VBUS    1
#define PWR_BATTERY 2

#define DISPLAY_OFF_TM  20 // seconds
#define VBUS_THRESHOLD  2500  // VBUS接続の有無を判別する閾値(mV)

// Instantiates a new Battery class
Battery::Battery() {}

/**
 * @brief バッテリのインスタンスを初期化
 */
void Battery::begin(int32_t posX, int32_t posY) {
  _pwrMode = PWR_UNKNOWN;
  lcdOn = true;
  _displayOffCnt = DISPLAY_OFF_TM;
  _posX = posX;
  _posY = posY;

  M5.Power.setExtOutput(false);
}

/**
 * @brief 電源状態のアップデート
 */
bool Battery::updatePowerState() {
  static bool toggle = false;
  bool lcdUpdateRequired = false;

  // M5.Display.setCursor(0, 28, 2);
  // M5.Display.printf("%d: %d mV     ", _displayOffCnt, M5.Power.getVBUSVoltage());

  // USBの挿抜に伴う処理
  if (M5.Power.getVBUSVoltage() > VBUS_THRESHOLD) {
    if (_pwrMode != PWR_VBUS) {
      _pwrMode = PWR_VBUS;
      if (!lcdOn) {
        wakeupLcd();
        lcdUpdateRequired = true;
      }
      // M5.Display.setBrightness(255);        
      M5.Power.setLed(true);
    }
    if (M5.Power.isCharging()) {
      toggle ^= true;
      M5.Power.setLed(toggle);
    } 
  } else {
    if (_pwrMode != PWR_BATTERY) {
      _pwrMode = PWR_BATTERY;
      M5.Power.setLed(false);
    }
    if (_displayOffCnt > 0) {
      _displayOffCnt--;
      if (_displayOffCnt == 4) {
        M5.Display.setBrightness(64);
      }
    }
    else {
      sleepLcd();
    }
  }

  // Powerボタンの押下に伴う処理
  if (M5.Power.getKeyState() == 0x02) {
    if (lcdOn) {
      sleepLcd();
    }
    else {
      wakeupLcd();
      lcdUpdateRequired = true;
    }
  }

  return lcdUpdateRequired;
}

/**
 * @brief LCDをウェイクアップさせる
 */
void Battery::wakeupLcd() {
  _displayOffCnt = DISPLAY_OFF_TM;

  if (!lcdOn) {
    // Turn LCD backlight on
    M5.Display.wakeup();
    // M5.Display.powerSaveOn();
    M5.Display.setBrightness(255);        
    lcdOn = true;
  }
}

/**
 * @brief LCDをスリープさせる
 */
void Battery::sleepLcd() {
  if (lcdOn) {
    // Turn LCD backlight off
    M5.Display.sleep();
    // M5.Display.powerSaveOff(); // これをするとなぜかそれ以降カラー表示ができなくなる
    lcdOn = false;
  }
}

/**
 * @brief バッテリの残量を表示する
 */
void Battery::showBatteryCapacity() {
  char buf[12];
  uint16_t color;
  int32_t capacity;

  capacity = M5.Power.getBatteryLevel();

  if (capacity >= 20) {
    color = GREEN;
  } else {
    color = RED;
  }

  // 電池のイメージ画像
  M5.Display.fillRect(_posX, _posY, 28, 16, BLACK);          // 消去
  M5.Display.drawRoundRect(_posX, _posY + 1, 26, 14, 3, WHITE);  // 電池外形
  M5.Display.drawLine(_posX + 26, _posY + 5, _posX + 26, _posY + 9, WHITE);  // プラス極の突起
  M5.Display.drawLine(_posX + 27, _posY + 6, _posX + 27, _posY + 8, WHITE);  // プラス極の突起
  M5.Display.fillRoundRect(_posX + 2, _posY + 3, capacity * 22 / 100, 10, 2, color);  // バッテリ残量

  // 充電の稲妻マーク
  if (M5.Power.isCharging()) {
    M5.Display.fillTriangle(_posX + 14, _posY, _posX + 9, _posY + 8, _posX + 13, _posY + 8, YELLOW);
    M5.Display.fillTriangle(_posX + 12, _posY + 15, _posX + 17, _posY + 8, _posX + 13, _posY + 8, YELLOW);
  }

  // 残量の％表示
  sprintf(buf, "  %d%%", capacity);
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.drawRightString(buf, _posX - 2, _posY, 2);
}
