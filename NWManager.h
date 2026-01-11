#ifndef NW_MANAGER_H
#define NW_MANAGER_H

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include "esp_sntp.h"
#include "Secrets.h"

#define NTP_SYNC_INTERVAL (24 * 60 * 60) // NTP synch interval (seconds)

class NWManager {
public:
    static void connectWiFi();
    static bool setupTime();
    static void processNtpSync(bool isVbusConnected, bool &wasVbusConnected);
    
private:
    static time_t lastNtpSyncTime;
};

#endif
