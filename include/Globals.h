// Globals.h - Global state, constants and forward declarations
// Adapted from CrowPanel SCD41 project for M5Stack
//
// June 2026 - Tetsu Nishimura
#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <M5Unified.h>
#include <lvgl.h>
#include <Preferences.h>
#include "Theme.h"

// ============================================================
// ボード設定 (解像度)
// ============================================================
static const uint16_t SCREEN_WIDTH  = 320;
static const uint16_t SCREEN_HEIGHT = 240;

// LVGL convenience aliases (int16_t to match lv_obj_set_size signature)
static const int16_t screenWidth  = 320;
static const int16_t screenHeight = 240;

// ============================================================
// 画面管理
// ============================================================
enum AppScreen {
    SCREEN_NONE,
    SCREEN_SENSOR,   // メイン: センサー値・グラフ
    SCREEN_WIFI,     // WiFi設定・接続
    SCREEN_MENU,     // メニュー
    SCREEN_DATESET,  // 日時設定
    SCREEN_TEST,     // テスト・デバッグ
};

// ============================================================
// アプリ状態
// ============================================================
struct AppState {
    AppScreen currentScreen;
    uint16_t  currentCO2;
    float     currentTemp;
    float     currentHumid;
    bool      sensorDataValid;
    bool      wifiConnecting;
    uint32_t  wifiStartTime;
    bool      bootConnecting;
    int       graphMode;   // 0=4H, 1=1D
    volatile bool needSDHistoryReload = false;
    volatile bool ntpUpdated = false;
};

extern AppState state;
extern Preferences prefs;

// ============================================================
// WiFi タイムアウト
// ============================================================
static const uint32_t WIFI_TIMEOUT_MS = 20000;

// ============================================================
// 画面遷移関数 (main.cpp で実装)
// ============================================================
void showWiFiScreen();
void showSensorScreen();
void showMenuScreen();
void showDateSetScreen();
void showTestScreen();

#endif // GLOBALS_H
