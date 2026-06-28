// HistoryManager.h - In-memory circular buffer for sensor history
// Ported from CrowPanel SCD41 project
//
// June 2026 - Tetsu Nishimura
#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include <Arduino.h>
#include <time.h>

// 4時間モード: 1分×240点
#define HISTORY_POINTS       240

// 24時間モード: 6分×240点
#define HISTORY_DAILY_POINTS 240

// ============================================================
// チャート用バッファ取得 API (idx=0が最古, idx=N-1が最新)
// ============================================================
uint16_t getHistCO2(int idx);
float    getHistTemp(int idx);
float    getHistHumid(int idx);

uint16_t getDailyHistCO2(int idx);
float    getDailyHistTemp(int idx);
float    getDailyHistHumid(int idx);

// ============================================================
// データ追加 API
// ============================================================
void addHistoryData(uint16_t co2, float temp, float humid);
void updateDailyHistoryInRealTime(uint16_t co2, float temp, float humid);

// ============================================================
// リセット・初期化 API (SD読み込み用)
// ============================================================
void resetHistory();
void resetDailyHistory();
void setDailyHistoryData(int idx, uint16_t co2, float temp, float humid);

#endif // HISTORYMANAGER_H
