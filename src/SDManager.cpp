// SDManager.cpp - SD card logging and history loading
// M5Stack Core2: GPIO4 (CS), default SPI, 25MHz
// Log format: /YYYYMMDD.csv, "YYYY-MM-DD HH:MM:SS, co2, temp, humid\n"
//
// June 2026 - Tetsu Nishimura

#include "SDManager.h"
#include "HistoryManager.h"
#include "Logger.h"
#include <SD.h>
#include <FS.h>

#define SD_CS_PIN GPIO_NUM_4
#define SD_FREQ   25000000

// ============================================================
// SDカード初期化
// ============================================================
void initSD() {
    SD.end();  // 既存マウントを確実に解除
    if (!SD.begin(SD_CS_PIN, SPI, SD_FREQ)) {
        LOG_E("SD", "Mount Failed");
    } else {
        LOG_I("SD", "Initialized. Type: %d, Size: %lluMB",
              SD.cardType(), SD.cardSize() / (1024 * 1024));
    }
}

// ============================================================
// ログ書き込み
// ファイル名: /YYYYMMDD.csv
// 形式: "YYYY-MM-DD HH:MM:SS, co2, temp, humid\n"
// ============================================================
void writeLogToSD(m5::rtc_datetime_t *rtcData,
                  uint16_t co2, float temperature, float humidity) {
    char logFileName[24];
    snprintf(logFileName, sizeof(logFileName), "/%04d%02d%02d.csv",
             rtcData->date.year, rtcData->date.month, rtcData->date.date);

    File file = SD.open(logFileName, FILE_APPEND);
    // SDカード抜き差し等でマウントが失われている場合は再初期化を試みる
    if (!file) {
        LOG_W("SD", "File open failed, trying to re-mount...");
        SD.end();
        delay(100);
        if (SD.begin(SD_CS_PIN, SPI, SD_FREQ)) {
            LOG_I("SD", "Re-mounted successfully");
            file = SD.open(logFileName, FILE_APPEND);
        }
    }
    if (file) {
        file.printf("%04d-%02d-%02d %02d:%02d:%02d, %d, %.2f, %.2f\n",
                    rtcData->date.year, rtcData->date.month, rtcData->date.date,
                    rtcData->time.hours, rtcData->time.minutes, rtcData->time.seconds,
                    co2, temperature, humidity);
        file.close();
        LOG_D("SD", "Log written: %s  CO2=%d T=%.1f H=%.1f",
              logFileName, co2, temperature, humidity);
    } else {
        LOG_E("SD", "Failed to open: %s", logFileName);
    }
}

// ============================================================
// 内部: ファイル1つを処理して4Hバッファに追加
// ============================================================
static void processLogFile4H(const char *logFileName,
                              time_t &t_cursor, time_t t_target_end) {
    File file = SD.open(logFileName, FILE_READ);
    if (!file) return;

    LOG_I("SD", "Loading 4H log: %s", logFileName);

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() < 20) continue;

        int year, month, day, hour, minute, second;
        int co2;
        float temp, humid;

        int items = sscanf(line.c_str(),
                           "%d-%d-%d %d:%d:%d, %d, %f, %f",
                           &year, &month, &day,
                           &hour, &minute, &second,
                           &co2, &temp, &humid);
        if (items < 9) continue;

        struct tm tm_line = {0};
        tm_line.tm_year  = year - 1900;
        tm_line.tm_mon   = month - 1;
        tm_line.tm_mday  = day;
        tm_line.tm_hour  = hour;
        tm_line.tm_min   = minute;
        tm_line.tm_sec   = 0;
        tm_line.tm_isdst = -1;

        time_t t_log = mktime(&tm_line);
        if (t_log < t_cursor) continue;
        if (t_log > t_target_end) break;

        // カーソルとログ時刻の間をゼロで埋める
        while (t_cursor < t_log) {
            addHistoryData(0, 0, 0);
            t_cursor += 60;
        }
        if (t_cursor == t_log) {
            addHistoryData(co2, temp, humid);
            t_cursor += 60;
        }
    }
    file.close();
}

// ============================================================
// 起動時: 4H分の履歴をSDから読み込む
// ============================================================
void loadHistoryFromSD(m5::rtc_datetime_t *now) {
    resetHistory();

    // JSTをシステム時刻として設定
    setenv("TZ", "JST-9", 1);
    tzset();

    struct tm tm_now = {0};
    tm_now.tm_year  = now->date.year - 1900;
    tm_now.tm_mon   = now->date.month - 1;
    tm_now.tm_mday  = now->date.date;
    tm_now.tm_hour  = now->time.hours;
    tm_now.tm_min   = now->time.minutes;
    tm_now.tm_sec   = 0;
    tm_now.tm_isdst = -1;

    time_t t_target_end = mktime(&tm_now);
    time_t t_scan       = t_target_end - (HISTORY_POINTS * 60);
    time_t t_cursor     = t_scan;
    time_t t_check      = t_scan;

    while (t_check <= t_target_end) {
        struct tm *tm_cur = localtime(&t_check);
        char logFileName[24];
        strftime(logFileName, sizeof(logFileName), "/%Y%m%d.csv", tm_cur);

        processLogFile4H(logFileName, t_cursor, t_target_end);

        // 翌日の00:00へ
        struct tm start_of_next = *tm_cur;
        start_of_next.tm_hour  = 0;
        start_of_next.tm_min   = 0;
        start_of_next.tm_sec   = 0;
        start_of_next.tm_mday += 1;
        start_of_next.tm_isdst = -1;
        t_check = mktime(&start_of_next);
    }

    // 末尾をゼロで埋める
    while (t_cursor <= t_target_end) {
        addHistoryData(0, 0, 0);
        t_cursor += 60;
    }
    LOG_I("SD", "History (4H) loaded.");
}

// ============================================================
// 起動時: 24H分の履歴をSDから読み込む
// ============================================================
void loadDailyHistoryFromSD(m5::rtc_datetime_t *now) {
    resetDailyHistory();

    setenv("TZ", "JST-9", 1);
    tzset();

    struct tm tm_now = {0};
    tm_now.tm_year  = now->date.year - 1900;
    tm_now.tm_mon   = now->date.month - 1;
    tm_now.tm_mday  = now->date.date;
    tm_now.tm_hour  = now->time.hours;
    tm_now.tm_min   = now->time.minutes;
    tm_now.tm_sec   = 0;
    tm_now.tm_isdst = -1;

    time_t t_now   = mktime(&tm_now);
    time_t t_start = t_now - (24 * 3600);

    // 前日・今日の2ファイルを読む
    uint32_t dailySumCO2[HISTORY_DAILY_POINTS]  = {0};
    float    dailySumTemp[HISTORY_DAILY_POINTS]  = {0};
    float    dailySumHumid[HISTORY_DAILY_POINTS] = {0};
    int      dailyCount[HISTORY_DAILY_POINTS]    = {0};

    for (int d = -1; d <= 0; d++) {
        time_t t_day = t_now + (d * 24 * 3600);
        struct tm *tm_day = localtime(&t_day);
        char logFileName[24];
        strftime(logFileName, sizeof(logFileName), "/%Y%m%d.csv", tm_day);

        File file = SD.open(logFileName, FILE_READ);
        if (!file) continue;
        LOG_I("SD", "Loading 1D log: %s", logFileName);

        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.length() < 20) continue;

            int year, month, day, hour, min, sec;
            int co2; float temp, humid;
            if (sscanf(line.c_str(),
                       "%d-%d-%d %d:%d:%d, %d, %f, %f",
                       &year, &month, &day, &hour, &min, &sec,
                       &co2, &temp, &humid) < 9) continue;

            struct tm tm_line = {0};
            tm_line.tm_year  = year - 1900;
            tm_line.tm_mon   = month - 1;
            tm_line.tm_mday  = day;
            tm_line.tm_hour  = hour;
            tm_line.tm_min   = min;
            tm_line.tm_sec   = sec;
            tm_line.tm_isdst = -1;
            time_t t_log = mktime(&tm_line);

            if (t_log < t_start || t_log > t_now) continue;

            long diff   = (long)(t_log - t_start);
            int bucket  = (int)(diff / (6 * 60));
            if (bucket >= 0 && bucket < HISTORY_DAILY_POINTS) {
                dailySumCO2[bucket]   += co2;
                dailySumTemp[bucket]  += temp;
                dailySumHumid[bucket] += humid;
                dailyCount[bucket]++;
            }
        }
        file.close();
    }

    for (int i = 0; i < HISTORY_DAILY_POINTS; i++) {
        if (dailyCount[i] > 0) {
            setDailyHistoryData(i,
                dailySumCO2[i]  / dailyCount[i],
                dailySumTemp[i]  / dailyCount[i],
                dailySumHumid[i] / dailyCount[i]);
        }
    }

    // SDロード後: リアルタイムモードのポインタを現在時刻バケツに同期させる。
    // これにより次回 updateDailyHistoryInRealTime() 呼び出し時の diff 爆発を防ぎ、
    // スリープ復帰後にグラフが全消去される不具合を修正する。
    int cur_bkt = (now->time.hours * 60 + now->time.minutes) / 6;
    initDailyHistoryRTMode(cur_bkt);

    LOG_I("SD", "History (1D) loaded.");
}
