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
#include <WiFi.h>
#include <SD.h>
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

SensirionI2cScd4x scd4x;
Battery battery;
Graph graph;
time_t lastNtpSyncTime = 0;

// Variables for accumulation and state tracking
int co2Sum = 0;
float tempSum = 0;
float humidSum = 0;
int numSum = 0;
bool wasVbusConnected = false;

const char WEEKDAY[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};
const char MONTH[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Color defined in RGB888 format
#define COLOR_CO2   (uint32_t)0x99ff99
#define COLOR_TEMP  (uint32_t)0xff9999
#define COLOR_HUMID (uint32_t)0x9999ff
#define NTP_SYNC_INTERVAL (24 * 60 * 60) // NTP同期の間隔(秒)

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
bool setupTime() {
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
    
    delay(1000);
    return true;
  }
  else {
    M5.Display.println("Failed to get NTP time");
    delay(1000);
    return false;
  }
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
    if (setupTime()) {
      lastNtpSyncTime = time(NULL);
    }

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
 * @brief 状態表示
 */
void showStatus(char *str, uint16_t scd4xError) {
  char strBuf[256];

  clearStatus();

  M5.Display.setCursor(0, 22, 2);
  M5.Display.setTextColor(TFT_RED);
  M5.Display.print(str);

  if (scd4xError) {
    errorToString(scd4xError, strBuf, 256);
    M5.Display.print(strBuf);
  }
}

/**
 * @brief 状態表示を消去
 */
void clearStatus() {
  M5.Display.setCursor(0, 22, 2);
  M5.Display.print("                                      ");
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

  openingSound();
  battery.begin(290, 0);
  scd4xInit();

  wifiConnection();

  // Draw display
  M5.Display.clear();

  M5.Display.setTextColor(COLOR_CO2);
  M5.Display.drawString("ppm", 80, 220, 2);
  M5.Display.setTextColor(COLOR_TEMP);
  M5.Display.drawCircle(190, 220, 2, COLOR_TEMP);
  M5.Display.drawString("C", 193, 220, 2);
  M5.Display.setTextColor(COLOR_HUMID);
  M5.Display.drawString("%", 280, 220, 2);
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
    char strBuf[256];

    clearStatus();

    // Read Measurement
    error = scd4x.readMeasurement(co2, temperature, humidity);
    if (error) {
      showStatus("Error: ", error);
    } else if (co2 == 0) {
      showStatus("Invalid sample detected", 0);
    } else {
      updateMeasurement(co2, temperature, humidity);

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
    updateTime(rtcData);

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
  time_t now = time(NULL);
  bool isVbusConnected = battery.isVbusConnected();
  if (isVbusConnected && !wasVbusConnected && (now - lastNtpSyncTime >= NTP_SYNC_INTERVAL)) {
    wifiConnection();
  }
  wasVbusConnected = isVbusConnected;
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
