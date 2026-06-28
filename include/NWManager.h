#ifndef NW_MANAGER_H
#define NW_MANAGER_H

#include <Arduino.h>

namespace NWManager {

    void connectIfVbus();
    void disconnectIfBattery();
    void checkWiFiStatus();
    void checkNTPStatus();
    void checkScanStatus();
    void syncNTP();
    bool isConnected();
    bool isConnecting();
    void bootConnect(const String &ssid, const String &pass);

} // namespace NWManager

#endif // NW_MANAGER_H
