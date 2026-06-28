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

#if ENABLE_DEBUG_LOG
    #define LOG_I(tag, fmt, ...) Serial.printf("[INFO][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define LOG_W(tag, fmt, ...) Serial.printf("[WARN][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define LOG_E(tag, fmt, ...) Serial.printf("[ERROR][%s] " fmt "\n", tag, ##__VA_ARGS__)
    #define LOG_D(tag, fmt, ...) Serial.printf("[DEBUG][%s] " fmt "\n", tag, ##__VA_ARGS__)
#else
    #define LOG_I(tag, fmt, ...)
    #define LOG_W(tag, fmt, ...)
    #define LOG_E(tag, fmt, ...)
    #define LOG_D(tag, fmt, ...)
#endif

// メモリ残量などのシステムヘルスを確認する特殊マクロ
#define LOG_SYS_HEALTH() \
    do { \
        if(ENABLE_DEBUG_LOG) { \
            Serial.printf("[SYS][HEALTH] Free Heap: %d bytes, Uptime: %lu ms\n", \
                          ESP.getFreeHeap(), millis()); \
        } \
    } while(0)

#endif // LOGGER_H
