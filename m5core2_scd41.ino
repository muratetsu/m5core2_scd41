// CO2 Sensor with Sensirion SCD41
// 最新のBoard/Libraryで作り直し．
//
// Board:
//   M5Stack 3.2.3
// Library:
//   M5GFX 0.2.15
//   M5Unified 0.2.10
//   Sensirion I2C SCD4x 1.1.0
//
// October 24, 2025
// Tetsu Nishimura

#include <Arduino.h>
#include <M5Unified.h>
#include <SensirionI2cScd4x.h>
#include <Wire.h>
#include <WiFi.h> // WiFi.mode in other places? No
#include <SD.h>
#include "Battery.h"
#include "Graph.h"
#include "NWManager.h"
#include "UIManager.h"

// Graph size
#define GRAPH_WIDTH 268
#define GRAPH_HIGHT 160
#define GRAPH_XMIN  26
#define GRAPH_YMIN  38
#define GRAPH_YGRID 20

SensirionI2cScd4x scd4x;
Battery battery;
Graph graph;

// Variables for accumulation and state tracking
int co2Sum = 0;
float tempSum = 0;
float humidSum = 0;
int numSum = 0;
bool wasVbusConnected = false;

// Note: WEEKDAY array was unused in original code, removed.
// MONTH array moved to UIManager.

/**
 * @brief Print SCD4x serial number
 */
void printSerialNumber(uint64_t serialNumber) {
  M5.Display.print("Serial: 0x");
  M5.Display.print((uint32_t)(serialNumber >> 32), HEX);
  M5.Display.print((uint32_t)(serialNumber & 0xFFFFFFFF), HEX);
  M5.Display.println();
}

/**
 * @brief Initialize and start SCD4x measurement
 */
 void scd4xInit(void) {
  int16_t error;
  char errorMessage[256];
  uint64_t serialNumber;

  scd4x.begin(Wire1, SCD41_I2C_ADDR_62);

  // Ensure sensor is in clean state
  error = scd4x.wakeUp();
  if (error) {
    M5.Display.print("Error trying to execute wakeUp(): ");
    errorToString(error, errorMessage, 256);
    M5.Display.println(errorMessage);
  }

  // stop potentially previously started measurement
  error = scd4x.stopPeriodicMeasurement();
  if (error) {
    M5.Display.print("Error trying to execute stopPeriodicMeasurement(): ");
    errorToString(error, errorMessage, 256);
    M5.Display.println(errorMessage);
  }

  error = scd4x.reinit();
  if (error) {
    M5.Display.print("Error trying to execute reinit(): ");
    errorToString(error, errorMessage, 256);
    M5.Display.println(errorMessage);
  }

  error = scd4x.getSerialNumber(serialNumber);
  if (error) {
    M5.Display.print("Error trying to execute getSerialNumber(): ");
    errorToString(error, errorMessage, 256);
    M5.Display.println(errorMessage);
  } else {
    printSerialNumber(serialNumber);
  }

  // Start Measurement
  error = scd4x.startPeriodicMeasurement();
  if (error) {
    M5.Display.print("Error trying to execute startPeriodicMeasurement(): ");
    errorToString(error, errorMessage, 256);
    M5.Display.println(errorMessage);
  }
}

/**
 * @brief SDカードにログを保存する
 */
void logging(char *filename, m5::rtc_datetime_t RTCdata, uint16_t co2, float temperature, float humidity) {
  File file = SD.open(filename, FILE_APPEND);
  if (file) {
    file.printf("%04d/%02d/%02d %02d:%02d:%02d,%d,%.1f,%.0f\r\n",
                RTCdata.date.year, RTCdata.date.month, RTCdata.date.date,
                RTCdata.time.hours, RTCdata.time.minutes, RTCdata.time.seconds,
                co2, temperature, humidity);
    file.close();
  } else {
    M5.Display.println("Failed to open log file.");
  }
}

/**
 * @brief Arduino setup
 */
void setup() {
  auto cfg = M5.config();
  cfg.output_power = false;
  cfg.internal_imu = false;
  cfg.internal_mic = false;
  M5.begin(cfg);
  
  // Initialize SD card
  while (!SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    M5.Display.println("SD Card Mount Failed");
    delay(1000);
  }
  M5.Display.setTextFont(4);
  M5.Display.println("Sensirion SCD41");
  M5.Display.setCursor(0, 40, 2);

  UIManager::openingSound();
  battery.begin(290, 0);
  scd4xInit();

  NWManager::connectWiFi();

  // Draw display
  UIManager::drawInitialLabels();
  graph.begin(GRAPH_WIDTH, GRAPH_HIGHT, GRAPH_XMIN, GRAPH_YMIN, GRAPH_YGRID, COLOR_CO2, COLOR_TEMP, COLOR_HUMID);
}

/**
 * @brief センサーデータの取得と蓄積 (5秒毎)
 */
void processSensorData(m5::rtc_datetime_t &rtcData) {
  static int prevSecond = 0;

  if ((rtcData.time.seconds - prevSecond + 60) % 60 >= 5) {
    prevSecond = rtcData.time.seconds;
    uint16_t co2;
    float temperature;
    float humidity;
    int16_t error;

    UIManager::clearStatus();

    // Read Measurement
    error = scd4x.readMeasurement(co2, temperature, humidity);
    if (error) {
      UIManager::showStatus("Error: ", error);
    } else if (co2 == 0) {
      UIManager::showStatus("Invalid sample detected", 0);
    } else {
      UIManager::updateMeasurement(co2, temperature, humidity);

      co2Sum += co2;
      tempSum += temperature;
      humidSum += humidity;
      numSum++;
    }
  }
}

/**
 * @brief 定期的な更新処理 (1分毎)
 * グラフ更新、ログ保存、平均値計算
 */
void processMinuteUpdate(m5::rtc_datetime_t &rtcData) {
  static int prevMinute = 0;

  if (prevMinute != rtcData.time.minutes) {
    prevMinute = rtcData.time.minutes;
    UIManager::updateTime(rtcData);

    if (numSum != 0) {
      uint16_t co2 = co2Sum / numSum;
      float temperature = tempSum / numSum;
      float humidity = humidSum / numSum;
      char strBuf[256];

      graph.add(co2, temperature, humidity);
      if (battery.lcdOn) {
        graph.plot(rtcData, 7);
        battery.showBatteryCapacity();
      }

      sprintf(strBuf, "/%d-%02d-%02d.csv", rtcData.date.year, rtcData.date.month, rtcData.date.date);
      logging(strBuf, rtcData, co2, temperature, humidity);

      co2Sum = 0;
      tempSum = 0;
      humidSum = 0;
      numSum = 0;
    }
  }
}

/**
 * @brief NTP同期の管理
 * VBUS接続時に24時間経過していれば同期する
 */
void processNtpSync() {
  bool isVbusConnected = battery.isVbusConnected();
  NWManager::processNtpSync(isVbusConnected, wasVbusConnected);
}

/**
 * @brief Arduino loop
 */
void loop() {
  m5::rtc_datetime_t RTCdata;
  
  RTCdata = M5.Rtc.getDateTime();

  // センサーデータ処理
  processSensorData(RTCdata);

  // 1分毎の更新処理
  processMinuteUpdate(RTCdata);

  // 電源状態の変化チェック
  if (battery.updatePowerState()) {   // USBが接続された時にLCDのアップデートが必要
    graph.plot(RTCdata, 7);
    battery.showBatteryCapacity();
  }

  // NTP同期確認
  processNtpSync();

  // light sleep
  M5.Power.lightSleep(1000000);
}
