#include "UIManager.h"

const char UIManager::MONTH[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void UIManager::drawInitialLabels() {
    M5.Display.clear();
    M5.Display.setTextColor(COLOR_CO2);
    M5.Display.drawString("ppm", 80, 220, 2);
    M5.Display.setTextColor(COLOR_TEMP);
    M5.Display.drawCircle(190, 220, 2, COLOR_TEMP);
    M5.Display.drawString("C", 193, 220, 2);
    M5.Display.setTextColor(COLOR_HUMID);
    M5.Display.drawString("%", 280, 220, 2);
}

void UIManager::updateTime(m5::rtc_datetime_t RTCdata) {
  char timeStrbuff[32];

  M5.Display.fillRect(0, 0, 200, 26, BLACK);  // Clear
  M5.Display.setTextColor(WHITE);
  sprintf(timeStrbuff, "%s %d   %d:%02d", MONTH[RTCdata.date.month - 1], RTCdata.date.date, RTCdata.time.hours, RTCdata.time.minutes);
  M5.Display.setTextDatum(0);
  M5.Display.drawString(timeStrbuff, 2, 0, 4);
}

void UIManager::updateMeasurement(uint16_t co2, float temperature, float humidity) {
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

void UIManager::showStatus(const char *str, uint16_t scd4xError) {
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

void UIManager::clearStatus() {
  M5.Display.fillRect(0, 22, M5.Display.width(), 16, BLACK);
}

void UIManager::openingSound() {
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
