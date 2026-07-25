// NWManager.cpp - WiFi & NTP management (USB-triggered)
// USB接続時のみWiFi接続し、切断時はWiFi OFFで省電力化
//
// June 2026 - Tetsu Nishimura

#include "NWManager.h"
#include "Globals.h"
#include "Logger.h"
#include "SDManager.h"
#include "Screen_WiFi.h"
#include "ota.h"
#include "SensorChart.h"
#include <WiFi.h>
#include <sys/time.h>
#include <esp_sntp.h>

// Forward declaration of the global checkScanStatus defined in Screen_WiFi.cpp
void checkScanStatus();

namespace NWManager {

    static bool ntpSyncing = false;
    static uint32_t ntpStartTime = 0;
    static m5::rtc_datetime_t rtcBeforeNTP;

    void connectIfVbus() {
        if (WiFi.status() == WL_CONNECTED || state.wifiConnecting) {
            return;
        }

        prefs.begin("wifi_cfg", true);
        String ssid = prefs.getString("ssid", "");
        String pass = prefs.getString("pass", "");
        prefs.end();

        if (ssid.length() == 0) {
            LOG_I("WiFi", "No WiFi SSID configured. Async connection skipped.");
            return;
        }

        LOG_I("WiFi", "VBUS connected. Starting async WiFi connection to SSID: %s", ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());
        state.wifiConnecting = true;
        state.wifiStartTime = millis();
    }

    void disconnectIfBattery() {
        if (WiFi.status() == WL_CONNECTED || state.wifiConnecting) {
            LOG_I("WiFi", "Battery mode. Disconnecting WiFi to save power.");
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            state.wifiConnecting = false;
            ntpSyncing = false;
        }
    }

    void checkWiFiStatus() {
        if (!state.wifiConnecting) {
            return;
        }

        wl_status_t status = WiFi.status();
        if (status == WL_CONNECTED) {
            state.wifiConnecting = false;
            LOG_I("WiFi", "WiFi Connected! IP: %s", WiFi.localIP().toString().c_str());
            
            syncNTP();
            ntpSyncing = true;
            ntpStartTime = millis();

            // Initialize OTA
            otaInit();
            otaScheduleFirstCheck();

            // Show Sensor Screen
            showSensorScreen();
        } else if (millis() - state.wifiStartTime > WIFI_TIMEOUT_MS) {
            state.wifiConnecting = false;
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            LOG_E("WiFi", "WiFi Connection Timeout.");
            
            if (state.bootConnecting) {
                state.bootConnecting = false;
                showMenuScreen();
            } else {
                setWiFiErrorLabel("[!] Connection failed. Check SSID/Password.");
            }
        }
    }

    void checkNTPStatus() {
        if (state.needSDHistoryReload) {
            state.needSDHistoryReload = false;
            m5::rtc_datetime_t now = M5.Rtc.getDateTime();
            loadHistoryFromSD(&now);
            loadDailyHistoryFromSD(&now);
        }

        if (state.ntpUpdated) {
            state.ntpUpdated = false;
            SensorChart_RefreshAll();
        }

        if (!ntpSyncing) {
            return;
        }

        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            ntpSyncing = false;
            LOG_I("NTP", "Initial NTP synchronization completed successfully.");
        } else if (millis() - ntpStartTime > WIFI_TIMEOUT_MS) {
            ntpSyncing = false;
            LOG_E("NTP", "Failed to obtain time (Timeout).");
        }
    }

    static void timeSyncCallback(struct timeval *tv) {
        LOG_I("NTP", "SNTP sync notification callback triggered.");
        
        time_t now = tv->tv_sec;
        struct tm timeInfo;
        localtime_r(&now, &timeInfo);
        
        m5::rtc_datetime_t rtcBefore = M5.Rtc.getDateTime();
        
        m5::rtc_datetime_t rtcData;
        rtcData.date.year = timeInfo.tm_year + 1900;
        rtcData.date.month = timeInfo.tm_mon + 1;
        rtcData.date.date = timeInfo.tm_mday;
        rtcData.date.weekDay = timeInfo.tm_wday;
        rtcData.time.hours = timeInfo.tm_hour;
        rtcData.time.minutes = timeInfo.tm_min;
        rtcData.time.seconds = timeInfo.tm_sec;
        M5.Rtc.setDateTime(rtcData);
        
        LOG_I("NTP", "RTC updated from NTP callback: %04d/%02d/%02d %02d:%02d:%02d",
              rtcData.date.year, rtcData.date.month, rtcData.date.date,
              rtcData.time.hours, rtcData.time.minutes, rtcData.time.seconds);
              
        struct tm tm_before = {0};
        tm_before.tm_year  = rtcBefore.date.year - 1900;
        tm_before.tm_mon   = rtcBefore.date.month - 1;
        tm_before.tm_mday  = rtcBefore.date.date;
        tm_before.tm_hour  = rtcBefore.time.hours;
        tm_before.tm_min   = rtcBefore.time.minutes;
        tm_before.tm_sec   = rtcBefore.time.seconds;
        tm_before.tm_isdst = -1;
        time_t t_before = mktime(&tm_before);
        
        long diff = (long)difftime(now, t_before);
        if (labs(diff) > 3600) { // 1時間以上の差
            state.needSDHistoryReload = true;
            LOG_I("NTP", "Time diff > 1h (%ld sec). Scheduled history reload from SD.", diff);
        } else {
            LOG_I("NTP", "Time diff small (%ld sec). Skipping SD reload.", diff);
        }
        
        state.ntpUpdated = true;
    }

    void syncNTP() {
        LOG_I("NTP", "NTP synchronization requested.");
        
        static bool cb_registered = false;
        if (!cb_registered) {
            sntp_set_time_sync_notification_cb(timeSyncCallback);
            cb_registered = true;
            LOG_I("NTP", "Time sync notification callback registered.");
        }
        
        // NTP開始前のRTC時刻を保存（後で時刻変化量を判定するため）
        rtcBeforeNTP = M5.Rtc.getDateTime();
        
        // M5 RTCの時刻をESP32内部RTCに設定（時刻表示が消えないように）
        syncSystemTimeWithRTC();

        configTime(0, 0, "ntp.nict.jp", "time.google.com");
        setenv("TZ", "JST-9", 1);
        tzset();
    }

    bool isConnected() {
        return WiFi.status() == WL_CONNECTED;
    }

    bool isConnecting() {
        return state.wifiConnecting;
    }

    void checkScanStatus() {
        // Forward to the global function in Screen_WiFi.cpp
        ::checkScanStatus();
    }

    void bootConnect(const String &ssid, const String &pass) {
        lv_obj_t *scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, THEME_BG_BLACK, 0);

        lv_obj_t *lbl = lv_label_create(scr);
        lv_label_set_text_fmt(lbl, LV_SYMBOL_WIFI "  Connecting to\n%s ...", ssid.c_str());
        lv_obj_set_style_text_color(lbl, lv_color_make(255, 220, 80), 0);
        lv_obj_center(lbl);
        lv_timer_handler();

        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());

        state.wifiConnecting = true;
        state.wifiStartTime  = millis();
        state.bootConnecting = true;

        LOG_I("Boot", "Auto-connecting to SSID: %s", ssid.c_str());
    }

    void syncSystemTimeWithRTC() {
        m5::rtc_datetime_t rtcNow = M5.Rtc.getDateTime();
        struct tm tm_rtc = {0};
        tm_rtc.tm_year  = rtcNow.date.year - 1900;
        tm_rtc.tm_mon   = rtcNow.date.month - 1;
        tm_rtc.tm_mday  = rtcNow.date.date;
        tm_rtc.tm_hour  = rtcNow.time.hours;
        tm_rtc.tm_min   = rtcNow.time.minutes;
        tm_rtc.tm_sec   = rtcNow.time.seconds;
        tm_rtc.tm_isdst = -1;
        time_t t_rtc = mktime(&tm_rtc);
        struct timeval tv = { t_rtc, 0 };
        settimeofday(&tv, NULL);
        
        // LOG_D("NTP", "System time synchronized with RTC: %04d/%02d/%02d %02d:%02d:%02d",
        //       rtcNow.date.year, rtcNow.date.month, rtcNow.date.date,
        //       rtcNow.time.hours, rtcNow.time.minutes, rtcNow.time.seconds);
    }

} // namespace NWManager
