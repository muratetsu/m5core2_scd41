// HistoryManager.cpp - In-memory circular buffer for sensor history
// Ported from CrowPanel SCD41 project
//
// June 2026 - Tetsu Nishimura

#include "HistoryManager.h"
#include "Logger.h"

// ============================================================
// 4Hバッファ (1分×240点)
// ============================================================
static uint16_t histCO2[HISTORY_POINTS]   = {0};
static float    histTemp[HISTORY_POINTS]  = {0};
static float    histHumid[HISTORY_POINTS] = {0};
static int      histIdx = 0;

// ============================================================
// 24Hバッファ (6分×240点)
// ============================================================
static uint16_t dailyHistCO2[HISTORY_DAILY_POINTS]   = {0};
static float    dailyHistTemp[HISTORY_DAILY_POINTS]  = {0};
static float    dailyHistHumid[HISTORY_DAILY_POINTS] = {0};
static int      dailyHistIdx = 0;

// ============================================================
// 4H バッファ読み取り (idx=0が最古, idx=N-1が最新)
// ============================================================
uint16_t getHistCO2(int idx) {
    int p = histIdx + idx;
    if (p >= HISTORY_POINTS) p -= HISTORY_POINTS;
    return histCO2[p];
}
float getHistTemp(int idx) {
    int p = histIdx + idx;
    if (p >= HISTORY_POINTS) p -= HISTORY_POINTS;
    return histTemp[p];
}
float getHistHumid(int idx) {
    int p = histIdx + idx;
    if (p >= HISTORY_POINTS) p -= HISTORY_POINTS;
    return histHumid[p];
}

// ============================================================
// 24H バッファ読み取り
// ============================================================
uint16_t getDailyHistCO2(int idx) {
    int p = dailyHistIdx + idx;
    if (p >= HISTORY_DAILY_POINTS) p -= HISTORY_DAILY_POINTS;
    return dailyHistCO2[p];
}
float getDailyHistTemp(int idx) {
    int p = dailyHistIdx + idx;
    if (p >= HISTORY_DAILY_POINTS) p -= HISTORY_DAILY_POINTS;
    return dailyHistTemp[p];
}
float getDailyHistHumid(int idx) {
    int p = dailyHistIdx + idx;
    if (p >= HISTORY_DAILY_POINTS) p -= HISTORY_DAILY_POINTS;
    return dailyHistHumid[p];
}

// ============================================================
// リセット
// ============================================================
void resetHistory() {
    histIdx = 0;
    memset(histCO2,   0, sizeof(histCO2));
    memset(histTemp,  0, sizeof(histTemp));
    memset(histHumid, 0, sizeof(histHumid));
}

void resetDailyHistory() {
    dailyHistIdx = 0;
    memset(dailyHistCO2,   0, sizeof(dailyHistCO2));
    memset(dailyHistTemp,  0, sizeof(dailyHistTemp));
    memset(dailyHistHumid, 0, sizeof(dailyHistHumid));
}

void setDailyHistoryData(int idx, uint16_t co2, float temp, float humid) {
    if (idx >= 0 && idx < HISTORY_DAILY_POINTS) {
        dailyHistCO2[idx]   = co2;
        dailyHistTemp[idx]  = temp;
        dailyHistHumid[idx] = humid;
    }
}

// ============================================================
// 4H バッファへのデータ追加 (1分毎)
// ============================================================
void addHistoryData(uint16_t co2, float temp, float humid) {
    histCO2[histIdx]   = co2;
    histTemp[histIdx]  = temp;
    histHumid[histIdx] = humid;
    histIdx = (histIdx + 1) % HISTORY_POINTS;
}

// ============================================================
// 24H バッファのリアルタイム更新 (6分バケツ)
// ============================================================
static uint32_t dailySumCO2_rt   = 0;
static float    dailySumTemp_rt  = 0;
static float    dailySumHumid_rt = 0;
static int      dailyCount_rt    = 0;
static int      _rtModeCurBucket = -1;

void updateDailyHistoryInRealTime(uint16_t co2, float temp, float humid) {
    if (co2 == 0) return;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return;

    // 6分単位のバケツ番号 (0〜239)
    int cur_bkt = (timeinfo.tm_hour * 60 + timeinfo.tm_min) / 6;

    if (_rtModeCurBucket == -1) {
        _rtModeCurBucket = cur_bkt;
        dailySumCO2_rt   = co2;
        dailySumTemp_rt  = temp;
        dailySumHumid_rt = humid;
        dailyCount_rt    = 1;
    } else if (_rtModeCurBucket == cur_bkt) {
        // 同じバケツに加算
        dailySumCO2_rt   += co2;
        dailySumTemp_rt  += temp;
        dailySumHumid_rt += humid;
        dailyCount_rt++;
    } else {
        // バケツが変わった: 空バケツを埋めてインデックスを進める
        int diff = (cur_bkt - _rtModeCurBucket + HISTORY_DAILY_POINTS) % HISTORY_DAILY_POINTS;
        if (diff > HISTORY_DAILY_POINTS) diff = HISTORY_DAILY_POINTS;

        for (int step = 0; step < diff; step++) {
            dailyHistCO2[dailyHistIdx]   = 0;
            dailyHistTemp[dailyHistIdx]  = 0;
            dailyHistHumid[dailyHistIdx] = 0;
            dailyHistIdx = (dailyHistIdx + 1) % HISTORY_DAILY_POINTS;
        }

        _rtModeCurBucket = cur_bkt;
        dailySumCO2_rt   = co2;
        dailySumTemp_rt  = temp;
        dailySumHumid_rt = humid;
        dailyCount_rt    = 1;
    }

    // 最新バケツに暫定平均を書き込む
    if (dailyCount_rt > 0) {
        int latestIdx = (dailyHistIdx + HISTORY_DAILY_POINTS - 1) % HISTORY_DAILY_POINTS;
        dailyHistCO2[latestIdx]   = dailySumCO2_rt   / dailyCount_rt;
        dailyHistTemp[latestIdx]  = dailySumTemp_rt  / dailyCount_rt;
        dailyHistHumid[latestIdx] = dailySumHumid_rt / dailyCount_rt;
    }
}
