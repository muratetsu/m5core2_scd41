// Battery Management
//
// November 1, 2025
// Tetsu Nishimura
#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include <M5Unified.h>

class Battery {
    public:
    Battery();
    void begin(int32_t posX, int32_t posY);
    bool updatePowerState();
    void showBatteryCapacity();
    bool lcdOn;

    private:
    void wakeupLcd();
    void sleepLcd();
    int _pwrMode;
    int _displayOffCnt;
    int32_t _posX;
    int32_t _posY;
};

#endif