#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <M5Unified.h>
#include <SensirionI2cScd4x.h> 

// Color defined in RGB888 format
#define COLOR_CO2   (uint32_t)0x99ff99
#define COLOR_TEMP  (uint32_t)0xff9999
#define COLOR_HUMID (uint32_t)0x9999ff

class UIManager {
public:
    static void drawInitialLabels();
    static void updateTime(m5::rtc_datetime_t RTCdata);
    static void updateMeasurement(uint16_t co2, float temperature, float humidity);
    static void showStatus(const char *str, uint16_t scd4xError);
    static void clearStatus();
    static void openingSound();
    
private:
    static const char MONTH[12][4];
};

#endif
