// Logger.h - Serial logging macros
// Ported from CrowPanel SCD41 project
//
// June 2026 - Tetsu Nishimura
#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// ============================================================
// ロギング設定
// 1にするとログがシリアルに出力されます。
// 0にすると全ログがコンパイル時に消去され、パフォーマンスが向上します。
// ============================================================
#define ENABLE_DEBUG_LOG 1
#define ENABLE_SD_LOG    1

// Helper function declared for writing system logs to SD
void writeSysLogFormatted(const char *level, const char *tag, const char *fmt, ...);

#if ENABLE_DEBUG_LOG
    #define LOG_I(tag, fmt, ...) do { Serial.printf("[INFO][%s] " fmt "\n", tag, ##__VA_ARGS__); writeSysLogFormatted("INFO", tag, fmt, ##__VA_ARGS__); } while(0)
    #define LOG_W(tag, fmt, ...) do { Serial.printf("[WARN][%s] " fmt "\n", tag, ##__VA_ARGS__); writeSysLogFormatted("WARN", tag, fmt, ##__VA_ARGS__); } while(0)
    #define LOG_E(tag, fmt, ...) do { Serial.printf("[ERROR][%s] " fmt "\n", tag, ##__VA_ARGS__); writeSysLogFormatted("ERROR", tag, fmt, ##__VA_ARGS__); } while(0)
    #define LOG_D(tag, fmt, ...) do { Serial.printf("[DEBUG][%s] " fmt "\n", tag, ##__VA_ARGS__); writeSysLogFormatted("DEBUG", tag, fmt, ##__VA_ARGS__); } while(0)
#else
    #define LOG_I(tag, fmt, ...)
    #define LOG_W(tag, fmt, ...)
    #define LOG_E(tag, fmt, ...)
    #define LOG_D(tag, fmt, ...)
#endif

// メモリ残量などのシステムヘルスを確認する特殊マクロ
#define LOG_SYS_HEALTH() \
    do { \
        LOG_I("SYS", "HEALTH Free Heap: %d bytes, Uptime: %lu ms", ESP.getFreeHeap(), millis()); \
    } while(0)

#endif // LOGGER_H
