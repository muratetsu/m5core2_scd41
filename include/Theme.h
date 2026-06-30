// Theme.h - UI color and constant definitions (LVGL / M5Unified)
// Adapted from CrowPanel SCD41 project for M5Stack
//
// June 2026 - Tetsu Nishimura
#ifndef THEME_H
#define THEME_H

#include <lvgl.h>

// ============================================================
// UI テーマ定義ファイル
// 画面の背景色やフォントカラー、センサーデータの色などを一元管理します
// ============================================================

// --- Background Colors ---
#define THEME_BG_LIGHT      lv_color_make(10,  15,  35)   // WiFi/OTA/チャート背景
#define THEME_BG_PANEL      lv_color_make(25,  30,  50)   // テスト用パネル背景
#define THEME_BG_BTN        lv_color_make(30,  35,  50)   // 汎用ボタン背景
#define THEME_BG_BLACK      lv_color_make(0,   0,   0)

// --- Text Colors ---
#define THEME_TEXT_WHITE    lv_color_make(255, 255, 255)
#define THEME_TEXT_LIGHT    lv_color_make(220, 230, 240)
#define THEME_TEXT_MUTED    lv_color_make(150, 160, 180)  // 補足テキスト用
#define THEME_TEXT_DARK     lv_color_make(100, 100, 100)  // 無効状態など
#define THEME_TEXT_TITLE    lv_color_make(120, 180, 255)  // 各画面タイトル用

// --- Sensor Data Colors ---
#define THEME_COLOR_CO2     lv_color_make(150, 255, 150)  // 緑
#define THEME_COLOR_TEMP    lv_color_make(255, 150, 150)  // 赤/ピンク
#define THEME_COLOR_HUMID   lv_color_make(150, 150, 255)  // 青

// --- Status Colors ---
#define THEME_COLOR_WARNING lv_color_make(255, 180,  50)  // OTA通知など
#define THEME_COLOR_ERROR   lv_color_make(220,  80,  80)
#define THEME_COLOR_GOOD    lv_color_make( 80, 220,  80)
#define THEME_COLOR_OK      lv_color_make(220, 200,  60)

// --- Borders and Separators ---
#define THEME_BORDER_DARK   lv_color_make( 60,  70,  90)  // チャート枠・グリッド
#define THEME_BORDER_LIGHT  lv_color_make( 60,  80, 130)  // メニュー区切り線など

// --- WiFi画面キーボード色 ---
#define THEME_KEY_BG        lv_color_make( 42,  48,  80)
#define THEME_KEY_PRESS     lv_color_make( 74,  96, 144)
#define THEME_KEY_SPECIAL   lv_color_make( 26,  32,  64)
#define THEME_KEY_OK        lv_color_make( 32,  88,  32)

// --- Battery Colors ---
#define THEME_BATTERY_OK    lv_color_make(  0, 204,   0)
#define THEME_BATTERY_LOW   lv_color_make(255,   0,   0)

#endif // THEME_H
