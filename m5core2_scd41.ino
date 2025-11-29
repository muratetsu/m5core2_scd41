// CO2 Sensor with Sensirion SCD41
// 最新のBoard/Libraryで作り直し．
//
// Board:
//   M5Stack 3.2.3
// Library:
//   M5GFX 0.2.15
//   M5Unified 0.2.10
//
// October 24, 2025
// Tetsu Nishimura

#include <Arduino.h>
#include <M5Unified.h>
#include <SensirionI2CScd4x.h>
#include <Wire.h>
#include <WiFi.h>
#include "esp_sntp.h"
#include "Battery.h"
#include "Graph.h"
#include "Secrets.h"

// Graph size
#define GRAPH_WIDTH 268
#define GRAPH_HIGHT 160
#define GRAPH_XMIN  26
#define GRAPH_YMIN  38
#define GRAPH_YGRID 20

SensirionI2CScd4x scd4x;
Battery battery;
Graph graph;

const char WEEKDAY[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};
const char MONTH[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Color defined in RGB888 format
#define COLOR_CO2   (uint32_t)0x99ff99
#define COLOR_TEMP  (uint32_t)0xff9999
#define COLOR_HUMID (uint32_t)0x9999ff

/**
 * @brief Print 16-bit unsigned number in Hex
 */
void printUint16Hex(uint16_t value) {
  M5.Display.print(value < 4096 ? "0" : "");
  M5.Display.print(value < 256 ? "0" : "");
  M5.Display.print(value < 16 ? "0" : "");
  M5.Display.print(value, HEX);
}

/**
 * @brief Print SCD4x serial number
 */
void printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2) {
  M5.Display.print("Serial: 0x");
  printUint16Hex(serial0);
  printUint16Hex(serial1);
  printUint16Hex(serial2);
  M5.Display.println();
}

/**
 * @brief Initialize and start SCD4x measurement
 */
 void scd4xInit(void) {
  uint16_t error;
  char errorMessage[256];
  uint16_t serial0;
  uint16_t serial1;
  uint16_t serial2;

  scd4x.begin(Wire1);

  // stop potentially previously started measurement
  error = scd4x.stopPeriodicMeasurement();
  if (error) {
    M5.Display.print("Error trying to execute stopPeriodicMeasurement(): ");
    errorToString(error, errorMessage, 256);
    M5.Display.println(errorMessage);
  }

  error = scd4x.getSerialNumber(serial0, serial1, serial2);
  if (error) {
    M5.Display.print("Error trying to execute getSerialNumber(): ");
    errorToString(error, errorMessage, 256);
    M5.Display.println(errorMessage);
  } else {
    printSerialNumber(serial0, serial1, serial2);
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
 * @brief 時刻表示の更新
 */
void updateTime(m5::rtc_datetime_t RTCdata) {
  char timeStrbuff[32];

  M5.Display.fillRect(0, 0, 200, 26, BLACK);  // 消去
  M5.Display.setTextColor(WHITE);
  sprintf(timeStrbuff, "%s %d   %d:%02d", MONTH[RTCdata.date.month - 1], RTCdata.date.date, RTCdata.time.hours, RTCdata.time.minutes);
  M5.Display.setTextDatum(0);
  M5.Display.drawString(timeStrbuff, 2, 0, 4);
}

/**
 * @brief 測定値の更新
 */
void updateMeasurement(uint16_t co2, float temperature, float humidity) {
  char buf[12];

  M5.Display.setTextDatum(2);

  // CO2
  M5.Display.setTextColor(COLOR_CO2, TFT_BLACK);
  sprintf(buf, "%6d", co2);
  M5.Display.drawRightString(buf, 75, 215, 4);
  // Temperature
  M5.Display.setTextColor(COLOR_TEMP, TFT_BLACK);
  sprintf(buf, "%5.1f", temperature);
  M5.Display.drawRightString(buf, 185, 215, 4);
  // Humidity
  M5.Display.setTextColor(COLOR_HUMID, TFT_BLACK);
  sprintf(buf, "%3.0f", humidity);
  M5.Display.drawRightString(buf, 275, 215, 4);
}

/**
 * @brief NTPを使った時刻補正
 */
void setupTime() {
  struct tm timeInfo;
  m5::rtc_datetime_t rtcData;

  configTzTime("JST-9", "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");

  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
    M5.Display.print(".");
    delay(500);
  }
  M5.Display.print("\n");

  if (getLocalTime(&timeInfo)) {
    // Set RTC time
    rtcData.date.year  = timeInfo.tm_year + 1900; // tm_yearは1900年からの年数
    rtcData.date.month = timeInfo.tm_mon + 1;     // tm_monは0から11
    rtcData.date.date  = timeInfo.tm_mday;
    rtcData.date.weekDay = timeInfo.tm_wday;      // tm_wdayはそのまま

    rtcData.time.hours   = timeInfo.tm_hour;
    rtcData.time.minutes = timeInfo.tm_min;
    rtcData.time.seconds = timeInfo.tm_sec;

    M5.Rtc.setDateTime(rtcData);

    M5.Display.printf("%04d/%02d/%02d %02d:%02d:%02d",
      rtcData.date.year, rtcData.date.month, rtcData.date.date,
      rtcData.time.hours, rtcData.time.minutes, rtcData.time.seconds);
  }
  else {
    M5.Display.println("Failed to get NTP time");
  }

  delay(1000);
}

/**
 * @brief Wi-Fi接続
 */
void wifiConnection() {
  // Establish WiFi connection
  M5.Display.println("Establish WiFi connection");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (int i = 0; i < 10; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
    else {
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
    setupTime();

    WiFi.disconnect(true);
  }

  WiFi.mode(WIFI_OFF);
}

/**
 * @brief 起動音鳴動
 */
void openingSound(void) {
  M5.Speaker.setVolume(128);
  M5.Speaker.tone(261.626, 1400, 1);  // tone 261.626Hz  output for 1 seconds, use channel 1
  M5.delay(200);
  M5.Speaker.tone(329.628, 1200, 2);  // tone 329.628Hz  output for 1 seconds, use channel 2
  M5.delay(200);
  M5.Speaker.tone(391.995, 1000, 3);  // tone 391.995Hz  output for 1 seconds, use channel 3
  M5.delay(400);

  for (int i = 128; i > 0; i--) {
    M5.Speaker.setVolume(i); // Volume can be changed during sound output.
    M5.delay(10);
  }
  M5.Speaker.stop();  // stop sound output.
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
  M5.Display.setTextFont(4);
  M5.Display.println("Sensirion SCD41");
  M5.Display.setCursor(0, 40, 2);

  openingSound();
  battery.begin(290, 0);
  scd4xInit();

  wifiConnection();

  // Draw display
  M5.Display.clear();

  M5.Display.setTextColor(COLOR_CO2);
  // M5.Lcd.drawString("CO2", 80, 208, 2);
  M5.Display.drawString("ppm", 80, 220, 2);
  M5.Display.setTextColor(COLOR_TEMP);
  // M5.Lcd.drawString("Temp", 190, 208, 2);
  M5.Display.drawCircle(190, 220, 2, COLOR_TEMP);
  M5.Display.drawString("C", 193, 220, 2);
  M5.Display.setTextColor(COLOR_HUMID);
  // M5.Lcd.drawString("Humid", 280, 208, 2);
  M5.Display.drawString("%", 280, 220, 2);
  graph.begin(GRAPH_WIDTH, GRAPH_HIGHT, GRAPH_XMIN, GRAPH_YMIN, GRAPH_YGRID, COLOR_CO2, COLOR_TEMP, COLOR_HUMID);
  
  // Wakeup用の1sタイマ
  esp_sleep_enable_timer_wakeup(1000000);
}

/**
 * @brief Arduino loop
 */
void loop() {
  static int prevMinute = 0;
  static int co2Sum = 0;
  static float tempSum = 0;
  static float humidSum = 0;
  static int numSum = 0;
  uint16_t co2;
  float temperature;
  float humidity;
  uint16_t error;
  char strBuf[256];
  m5::rtc_datetime_t RTCdata;
  
  RTCdata = M5.Rtc.getDateTime();

  // 5秒ごとに測定を実施
  if (RTCdata.time.seconds % 5 == 0) {
    // Read Measurement
    error = scd4x.readMeasurement(co2, temperature, humidity);
    if (error) {
      M5.Display.setCursor(0, 120, 2);
      M5.Display.print("Error trying to execute readMeasurement(): ");
      errorToString(error, strBuf, 256);
      M5.Display.println(strBuf);
    } else if (co2 == 0) {
      M5.Display.println("Invalid sample detected, skipping.");
    } else {
      updateMeasurement(co2, temperature, humidity);

      co2Sum += co2;
      tempSum += temperature;
      humidSum += humidity;
      numSum++;
    }
  }

  //  1分ごとにグラフを更新
  if (prevMinute != RTCdata.time.minutes) {
    prevMinute = RTCdata.time.minutes;
    updateTime(RTCdata);

    if (numSum != 0) {
      co2 = co2Sum / numSum;
      temperature = tempSum / numSum;
      humidity = humidSum / numSum;

      graph.add(co2, temperature, humidity);
      if (battery.lcdOn) {
        graph.plot(RTCdata, 7);
        battery.showBatteryCapacity();
      }

      sprintf(strBuf, "/%d-%02d-%02d.csv", RTCdata.date.year, RTCdata.date.month, RTCdata.date.date);
      // logging(strBuf, RTCdate, RTCtime, co2, temperature, humidity);

      co2Sum = 0;
      tempSum = 0;
      humidSum = 0;
      numSum = 0;
    }
  }

  if (battery.updatePowerState()) {   // USBが接続された時にLCDのアップデートが必要
    graph.plot(RTCdata, 7);
    battery.showBatteryCapacity();
  }

  // light sleep (TODO: 5M.PowerのtimerSleep/lightSleep/deepSleepに置き換え可能か検討)
  esp_light_sleep_start();
}
