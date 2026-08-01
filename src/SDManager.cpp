// SDManager.cpp - SD card logging and history loading
// M5Stack Core2: GPIO4 (CS), default SPI, 25MHz
// Log format: /YYYYMMDD.csv, "YYYY-MM-DD HH:MM:SS, co2, temp, humid\n"
//
// June 2026 - Tetsu Nishimura

#include "SDManager.h"
#include "HistoryManager.h"
#include "Logger.h"
#include "Globals.h"
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
//
// バケツ座標系: updateDailyHistoryInRealTime() と同一の絶対座標を使用
//   abs_bkt = (hour * 60 + min) / 6  (0〜239, 1日のどの6分枠か)
//
// 24Hウィンドウの構成:
//   dailyHistCO2[abs_bkt]  でバッファに直接配置
//   ・前日データ: abs_bkt > cur_bkt_now  (24H前〜今日0時前)
//   ・当日データ: abs_bkt <= cur_bkt_now (今日0時〜現在)
//
// これにより initDailyHistoryRTMode(cur_bkt_now) 後のバッファ配置が
// RTモードの書き込みと一致し、正しい時系列でグラフ表示される。
// ============================================================
void loadDailyHistoryFromSD(m5::rtc_datetime_t *now) {
    resetDailyHistory();

    setenv("TZ", "JST-9", 1);
    tzset();

    // 現在の絶対バケツ番号 (RTモードと同一座標系)
    int cur_bkt_now = (now->time.hours * 60 + now->time.minutes) / 6;

    // バケツ毎の集計バッファ (インデックス = 絶対バケツ番号)
    uint32_t sumCO2[HISTORY_DAILY_POINTS]   = {0};
    float    sumTemp[HISTORY_DAILY_POINTS]  = {0.0f};
    float    sumHumid[HISTORY_DAILY_POINTS] = {0.0f};
    int      cnt[HISTORY_DAILY_POINTS]      = {0};

    struct tm tm_now = {0};
    tm_now.tm_year  = now->date.year - 1900;
    tm_now.tm_mon   = now->date.month - 1;
    tm_now.tm_mday  = now->date.date;
    tm_now.tm_hour  = now->time.hours;
    tm_now.tm_min   = now->time.minutes;
    tm_now.tm_sec   = 0;
    tm_now.tm_isdst = -1;
    time_t t_now = mktime(&tm_now);

    // 前日 (d=-1) と当日 (d=0) の2ファイルを読む
    for (int d = -1; d <= 0; d++) {
        time_t t_day = t_now + (long)(d * 24 * 3600);
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

            // RTモードと同一の絶対バケツ番号
            int abs_bkt = (hour * 60 + min) / 6;

            // 24Hウィンドウへの採用判定:
            //   前日 (d=-1): abs_bkt > cur_bkt_now のみ (24H前〜当日0時前が対象)
            //   当日 (d= 0): abs_bkt <= cur_bkt_now のみ (当日0時〜現在が対象)
            bool valid = (d == -1) ? (abs_bkt > cur_bkt_now)
                                   : (abs_bkt <= cur_bkt_now);
            if (!valid) continue;

            sumCO2[abs_bkt]   += (uint32_t)co2;
            sumTemp[abs_bkt]  += temp;
            sumHumid[abs_bkt] += humid;
            cnt[abs_bkt]++;
        }
        file.close();
    }

    // 各絶対バケツに平均値を書き込む
    for (int i = 0; i < HISTORY_DAILY_POINTS; i++) {
        if (cnt[i] > 0) {
            setDailyHistoryData(i,
                (uint16_t)(sumCO2[i] / cnt[i]),
                sumTemp[i]  / cnt[i],
                sumHumid[i] / cnt[i]);
        }
    }

    // バッファポインタをRTモードと同期 (座標系が一致しているので dailyHistIdx も正しく設定される)
    initDailyHistoryRTMode(cur_bkt_now);

    LOG_I("SD", "History (1D) loaded. cur_bkt_now=%d", cur_bkt_now);
}

// ============================================================
// システムログのSD書き込み & ローテーション管理
// ============================================================
static bool s_sdSysLogEnabled = true;
static bool s_sdSysLogPreferencesLoaded = false;
static bool s_inWriteSysLog = false; // 再帰呼び出し防止フラグ

bool isSDSysLogEnabled() {
    if (!s_sdSysLogPreferencesLoaded) {
        prefs.begin("sys_log", true);
        s_sdSysLogEnabled = prefs.getBool("enabled", true);
        prefs.end();
        s_sdSysLogPreferencesLoaded = true;
    }
    return s_sdSysLogEnabled;
}

void setSDSysLogEnabled(bool enabled) {
    s_sdSysLogEnabled = enabled;
    s_sdSysLogPreferencesLoaded = true;
    prefs.begin("sys_log", false);
    prefs.putBool("enabled", enabled);
    prefs.end();
}

#define SYS_LOG_BUFFER_SIZE 4096
static char s_sysLogBuffer[SYS_LOG_BUFFER_SIZE];
static size_t s_sysLogHead = 0;
static portMUX_TYPE s_logMux = portMUX_INITIALIZER_UNLOCKED;

void flushSysLogBuffer() {
    if (!isSDSysLogEnabled()) return;
    if (s_sysLogHead == 0) return;

    portENTER_CRITICAL(&s_logMux);
    char tempBuf[SYS_LOG_BUFFER_SIZE];
    size_t copyLen = s_sysLogHead;
    memcpy(tempBuf, s_sysLogBuffer, copyLen);
    s_sysLogHead = 0;
    portEXIT_CRITICAL(&s_logMux);

    m5::rtc_datetime_t now = M5.Rtc.getDateTime();
    char logFileName[32];
    snprintf(logFileName, sizeof(logFileName), "/sys_%04d%02d%02d.log",
             now.date.year, now.date.month, now.date.date);

    File file = SD.open(logFileName, FILE_APPEND);
    if (file) {
        file.write((const uint8_t *)tempBuf, copyLen);
        file.flush();
        file.close();
    }
}

void writeSysLogToSD(const char *level, const char *tag, const char *message) {
    if (!isSDSysLogEnabled()) return;
    if (s_inWriteSysLog) return;
    s_inWriteSysLog = true;

    m5::rtc_datetime_t now = M5.Rtc.getDateTime();
    char line[300];
    int len = snprintf(line, sizeof(line), "[%04d-%02d-%02d %02d:%02d:%02d][%s][%s] %s\n",
                       now.date.year, now.date.month, now.date.date,
                       now.time.hours, now.time.minutes, now.time.seconds,
                       level, tag, message);

    if (len > 0) {
        portENTER_CRITICAL(&s_logMux);
        if (s_sysLogHead + len < SYS_LOG_BUFFER_SIZE) {
            memcpy(s_sysLogBuffer + s_sysLogHead, line, len);
            s_sysLogHead += len;
        } else {
            // バッファフル時は溢れる前に自動フラッシュ
            portEXIT_CRITICAL(&s_logMux);
            flushSysLogBuffer();
            portENTER_CRITICAL(&s_logMux);
            if (s_sysLogHead + len < SYS_LOG_BUFFER_SIZE) {
                memcpy(s_sysLogBuffer + s_sysLogHead, line, len);
                s_sysLogHead += len;
            }
        }
        portEXIT_CRITICAL(&s_logMux);
    }
    s_inWriteSysLog = false;
}

void writeSysLogFormatted(const char *level, const char *tag, const char *fmt, ...) {
    if (!isSDSysLogEnabled()) return;
    if (s_inWriteSysLog) return;

    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    writeSysLogToSD(level, tag, buf);
}

// 7日以上経過した /sys_YYYYMMDD.log ファイルの自動削除
void cleanOldSysLogs(m5::rtc_datetime_t *now) {
    File root = SD.open("/");
    if (!root || !root.isDirectory()) return;

    struct tm tm_now = {0};
    tm_now.tm_year  = now->date.year - 1900;
    tm_now.tm_mon   = now->date.month - 1;
    tm_now.tm_mday  = now->date.date;
    tm_now.tm_hour  = 0;
    tm_now.tm_min   = 0;
    tm_now.tm_sec   = 0;
    tm_now.tm_isdst = -1;
    time_t t_now = mktime(&tm_now);

    File file = root.openNextFile();
    while (file) {
        String fname = String(file.name());
        // fname は "sys_20260801.log" または "/sys_20260801.log"
        int idx = fname.lastIndexOf("sys_");
        if (idx != -1 && fname.endsWith(".log") && fname.length() >= idx + 12) {
            String dateStr = fname.substring(idx + 4, idx + 12);
            if (dateStr.length() == 8) {
                int year = dateStr.substring(0, 4).toInt();
                int month = dateStr.substring(4, 6).toInt();
                int day = dateStr.substring(6, 8).toInt();

                struct tm tm_file = {0};
                tm_file.tm_year  = year - 1900;
                tm_file.tm_mon   = month - 1;
                tm_file.tm_mday  = day;
                tm_file.tm_isdst = -1;
                time_t t_file = mktime(&tm_file);

                double diffSec = difftime(t_now, t_file);
                // 7日 (7 * 24 * 3600 秒) 以上前のログを削除
                if (diffSec >= (7 * 24 * 3600)) {
                    file.close();
                    String fullPath = fname.startsWith("/") ? fname : "/" + fname;
                    if (SD.remove(fullPath)) {
                        LOG_I("SD", "Removed old syslog file: %s", fullPath.c_str());
                    } else {
                        LOG_E("SD", "Failed to remove syslog file: %s", fullPath.c_str());
                    }
                    file = root.openNextFile();
                    continue;
                }
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
}

