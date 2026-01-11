#include "NWManager.h"

time_t NWManager::lastNtpSyncTime = 0;

void NWManager::connectWiFi() {
    // Establish WiFi connection
    M5.Display.println("Establish WiFi connection");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    for (int i = 0; i < 10; i++) {
        if (WiFi.status() == WL_CONNECTED) {
            break;
        } else {
            delay(500);
            M5.Display.print('.');
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        M5.Display.print("\r\nWiFi connected\r\nIP address: ");
        M5.Display.println(WiFi.localIP());
        delay(500);

        // setup time
        M5.Display.println("Setup time");
        if (setupTime()) {
            lastNtpSyncTime = time(NULL);
        }

        WiFi.disconnect(true);
    }

    WiFi.mode(WIFI_OFF);
}

bool NWManager::setupTime() {
    struct tm timeInfo;
    m5::rtc_datetime_t rtcData;
    uint32_t startMs = millis();
    const uint32_t timeoutMs = 20000;

    configTzTime("JST-9", "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");

    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        if (millis() - startMs > timeoutMs) {
            M5.Display.println("\nNTP Sync Timeout");
            return false;
        }
        M5.Display.print(".");
        delay(500);
    }
    M5.Display.print("\n");

    if (getLocalTime(&timeInfo)) {
        // Set RTC time
        rtcData.date.year = timeInfo.tm_year + 1900;
        rtcData.date.month = timeInfo.tm_mon + 1;
        rtcData.date.date = timeInfo.tm_mday;
        rtcData.date.weekDay = timeInfo.tm_wday;

        rtcData.time.hours = timeInfo.tm_hour;
        rtcData.time.minutes = timeInfo.tm_min;
        rtcData.time.seconds = timeInfo.tm_sec;

        M5.Rtc.setDateTime(rtcData);

        M5.Display.printf("%04d/%02d/%02d %02d:%02d:%02d",
                          rtcData.date.year, rtcData.date.month, rtcData.date.date,
                          rtcData.time.hours, rtcData.time.minutes, rtcData.time.seconds);

        delay(1000);
        return true;
    } else {
        M5.Display.println("Failed to get NTP time");
        delay(1000);
        return false;
    }
}

void NWManager::processNtpSync(bool isVbusConnected, bool &wasVbusConnected) {
    time_t now = time(NULL);
    if (isVbusConnected && !wasVbusConnected && (now - lastNtpSyncTime >= NTP_SYNC_INTERVAL)) {
        connectWiFi();
    }
    wasVbusConnected = isVbusConnected;
}
