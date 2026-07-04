// main.cpp - M5Stack Core2 SCD41 Sensor Logger Main Application
//
// June 2026 - Tetsu Nishimura

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <lvgl.h>
#include "Globals.h"
#include "Theme.h"
#include "Logger.h"
#include "HistoryManager.h"
#include "SensorManager.h"
#include "SDManager.h"
#include "Battery.h"
#include "NWManager.h"
#include "Screen_Sensor.h"
#include "Screen_WiFi.h"
#include "Screen_Menu.h"
#include "Screen_DateSet.h"
#include "Screen_Test.h"
#include "Screen_OTA.h"
#include "ota.h"
#include "SensorChart.h"

// Define global state instances
AppState state;
Preferences prefs;
Battery battery;

// Variables for accumulation and 1-minute aggregation
static uint32_t aggSumCO2 = 0;
static float aggSumTemp = 0.0f;
static float aggSumHumid = 0.0f;
static int aggNumSamples = 0;

static uint32_t lastDateTimeUpdate = 0;
static bool wasVbusConnected = false;

// Backlight Constants
#define BRIGHTNESS_DAY      128
#define BRIGHTNESS_NIGHT    16
#define BACKLIGHT_HOUR_DAWN 6
#define BACKLIGHT_HOUR_DUSK 22

// Screen switching definitions
void showWiFiScreen() {
    resetWiFiUI_Fields();
    state.currentScreen = SCREEN_WIFI;

    lv_obj_t *scr = lv_obj_create(NULL);
    createWiFiUI(scr);
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
}

void showSensorScreen() {
    resetSensorUI_Fields();
    state.currentScreen = SCREEN_SENSOR;

    lv_obj_t *scr = lv_obj_create(NULL);
    createSensorUI(scr);
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
}

void showMenuScreen() {
    resetSensorUI_Fields();
    state.currentScreen = SCREEN_MENU;

    lv_obj_t *scr = lv_obj_create(NULL);
    createMenuUI(scr);
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
}

void showDateSetScreen() {
    resetDateSetUI_Fields();
    state.currentScreen = SCREEN_DATESET;

    lv_obj_t *scr = lv_obj_create(NULL);
    createDateSetUI(scr);
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
}

void showTestScreen() {
    state.currentScreen = SCREEN_TEST;

    lv_obj_t *scr = lv_obj_create(NULL);
    createTestUI(scr);
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, true);
}

// Speaker Opening Chime
void openingSound() {
    M5.Speaker.setVolume(128);
    M5.Speaker.tone(261.626, 1400, 1);  // tone 261.626Hz
    M5.delay(200);
    M5.Speaker.tone(329.628, 1200, 2);  // tone 329.628Hz
    M5.delay(200);
    M5.Speaker.tone(391.995, 1000, 3);  // tone 391.995Hz
    M5.delay(400);
    
    for (int i = 128; i > 0; i--) {
        M5.Speaker.setVolume(i);
        M5.delay(10);
    }
    M5.Speaker.stop();
}

// ============================================================
// LVGL Backend Drivers (M5GFX / M5Unified)
// ============================================================

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[320 * 20]; // 20 lines display buffer

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    M5.Display.pushImage(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    auto detail = M5.Touch.getDetail(0);
    if (detail.isPressed()) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = detail.x;
        data->point.y = detail.y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void lvgl_init() {
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, 320 * 20);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

// ============================================================
// Backlight Brightness Control
// ============================================================

void updateBacklightBrightness() {
    m5::rtc_datetime_t now = M5.Rtc.getDateTime();
    int hour = now.time.hours;
    if (hour >= BACKLIGHT_HOUR_DUSK || hour < BACKLIGHT_HOUR_DAWN) {
        M5.Display.setBrightness(BRIGHTNESS_NIGHT);
        LOG_D("Backlight", "Night mode (Brightness: %d)", BRIGHTNESS_NIGHT);
    } else {
        M5.Display.setBrightness(BRIGHTNESS_DAY);
        LOG_D("Backlight", "Day mode (Brightness: %d)", BRIGHTNESS_DAY);
    }
}

// ============================================================
// Sensor Readings & Aggregation
// ============================================================

void processSensorData() {
    uint16_t co2 = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;

    int status = SensorManager::readData(co2, temperature, humidity);
    
    if (status == 1) {
        state.currentCO2 = co2;
        state.currentTemp = temperature;
        state.currentHumid = humidity;
        state.sensorDataValid = true;
        
        aggSumCO2 += co2;
        aggSumTemp += temperature;
        aggSumHumid += humidity;
        aggNumSamples++;
    } else if (status == -1) {
        state.sensorDataValid = false;
    }
}

void processMinuteAggregation() {
    m5::rtc_datetime_t now = M5.Rtc.getDateTime();

    if (SensorManager::isAggregationTime(now)) {
        if (aggNumSamples > 0) {
            uint16_t avgCO2 = aggSumCO2 / aggNumSamples;
            float avgTemp = aggSumTemp / aggNumSamples;
            float avgHumid = aggSumHumid / aggNumSamples;
            
            bool warmingUp = SensorManager::isWarmingUp();
            uint16_t plotCO2 = warmingUp ? 0 : avgCO2;
            float plotTemp = warmingUp ? 0.0f : avgTemp;
            float plotHumid = warmingUp ? 0.0f : avgHumid;

            addHistoryData(plotCO2, plotTemp, plotHumid);
            updateDailyHistoryInRealTime(plotCO2, plotTemp, plotHumid);
            
            updateSensorChartData(plotCO2, plotTemp, plotHumid);
            
            if (!warmingUp) {
                writeLogToSD(&now, avgCO2, avgTemp, avgHumid);
            } else {
                LOG_D("SD", "Skip writing log to SD (warming up)");
            }

            LOG_D("Graph", "Aggregated -> CO2: %d, Temp: %.1f, Humid: %.1f (Samples: %d)", 
                          avgCO2, avgTemp, avgHumid, aggNumSamples);

            LOG_SYS_HEALTH();

            aggSumCO2 = 0;
            aggSumTemp = 0.0f;
            aggSumHumid = 0.0f;
            aggNumSamples = 0;

            updateBacklightBrightness();
        }
    }
}

// ============================================================
// Setup & Loop
// ============================================================

void setup() {
    // 1. M5 Initialize
    auto cfg = M5.config();
    cfg.output_power = false;
    cfg.internal_imu = false;
    cfg.internal_mic = false;
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
    
    LOG_I("Boot", "M5Stack SCD41 Sensor starting up (LVGL Mode)...");

    // 2. Initialize SD card
    initSD();

    // 3. Initialize Battery and Power Manager
    battery.begin(280, 4);

    // 4. Initialize speaker Opening chime
    openingSound();

    // 5. Initialize LVGL
    lvgl_init();

    // 6. Initialize SCD41 Sensor
    SensorManager::init();

    // 7. Load local history from SD based on current RTC time
    m5::rtc_datetime_t now = M5.Rtc.getDateTime();
    loadHistoryFromSD(&now);
    loadDailyHistoryFromSD(&now);

    // 8. Establish initial WiFi / USB connection state
    wasVbusConnected = battery.isVbusConnected();
    if (wasVbusConnected) {
        NWManager::connectIfVbus();
    } else {
        NWManager::disconnectIfBattery();
    }

    // 9. Load saved WiFi credentials & auto-connect
    prefs.begin("wifi_cfg", true);
    String savedSSID = prefs.getString("ssid", "");
    String savedPass = prefs.getString("pass", "");
    prefs.end();

    if (savedSSID.length() == 0) {
        LOG_I("Boot", "No saved credentials. -> Date/Time set screen.");
        showDateSetScreen();
    } else {
        LOG_I("Boot", "Found saved SSID: %s -> Auto-connecting.", savedSSID.c_str());
        NWManager::bootConnect(savedSSID, savedPass);
    }

    LOG_I("Boot", "Setup complete.");
}

void loop() {
    M5.update();
    lv_timer_handler();   // LVGL tick and event processing

    // 1. Process network WiFi / NTP checks
    NWManager::checkWiFiStatus();
    NWManager::checkNTPStatus();
    NWManager::checkScanStatus();
    otaLoop();

    // 2. Check for VBUS/USB power status changes
    bool isVbus = battery.isVbusConnected();
    if (isVbus != wasVbusConnected) {
        wasVbusConnected = isVbus;
        if (isVbus) {
            NWManager::connectIfVbus();
        } else {
            NWManager::disconnectIfBattery();
        }
    }

    // 3. Update battery power management cycle
    if (battery.updatePowerState()) {
        // Redraw screen if screen just woke up
        if (state.currentScreen == SCREEN_SENSOR) {
            showSensorScreen();
        } else if (state.currentScreen == SCREEN_WIFI) {
            showWiFiScreen();
        }
    }

    // 4. Poll sensor readings
    processSensorData();

    // 5. Update sensor label display periodically
    if (state.currentScreen == SCREEN_SENSOR && battery.lcdOn) {
        if (millis() - lastDateTimeUpdate >= 1000) {
            lastDateTimeUpdate = millis();
            updateSensorLabel();
        }
    }

    // 6. Aggregate data once per minute
    processMinuteAggregation();

    // 7. Light Sleep Cycle (only when USB is disconnected and WiFi is idle)
    if (!isVbus && !state.wifiConnecting && WiFi.scanComplete() != WIFI_SCAN_RUNNING && WiFi.status() != WL_CONNECTED) {
        M5.Power.lightSleep(1000000); // sleep 1s
    } else {
        delay(5);
    }
}
