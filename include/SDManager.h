// SDManager.h - SD card logging and history loading
// Adapted from CrowPanel SCD41 project for M5Stack
//
// June 2026 - Tetsu Nishimura
#ifndef SDMANAGER_H
#define SDMANAGER_H

#include <Arduino.h>
#include <time.h>
#include <M5Unified.h>

// SDカード初期化
void initSD();

// センサーログをSDに書き込む
// ファイル名: /YYYYMMDD.csv
// 形式: "YYYY-MM-DD HH:MM:SS, co2, temp, humid\n"
void writeLogToSD(m5::rtc_datetime_t *rtcData, uint16_t co2, float temperature, float humidity);

// 起動時にSDの過去ログをHistoryManagerに読み込む (4H分)
void loadHistoryFromSD(m5::rtc_datetime_t *now);

// 起動時にSDの過去ログをHistoryManagerに読み込む (24H分)
void loadDailyHistoryFromSD(m5::rtc_datetime_t *now);

// システムログをSDに書き込む (/sys_YYYYMMDD.log)
void writeSysLogToSD(const char *level, const char *tag, const char *message);

// 7日以上前のシステムログファイルを削除する
void cleanOldSysLogs(m5::rtc_datetime_t *now);

// SDシステムロギングのON/OFF制御・状態取得
bool isSDSysLogEnabled();
void setSDSysLogEnabled(bool enabled);

#endif // SDMANAGER_H
