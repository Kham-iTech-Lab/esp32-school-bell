/*
  ESP32 กริ่งโรงเรียนดังเองตามคาบเรียน
  โค้ดประกอบคลิป K iTech Review / Kham iTech Lab

  หลักการ:
    1) ESP32 ดึงเวลาจากอินเทอร์เน็ต (NTP) เวลาไทย UTC+7
    2) ใส่ตารางคาบไว้ใน BELL_TIMES
    3) ถึงเวลา สั่งรีเลย์ให้กริ่งดัง RING_SECONDS วินาทีแล้วตัด
    4) มีโมดูลนาฬิกา DS3231 (ไม่บังคับ) เผื่อวันที่เน็ตล่ม เวลาก็ยังเดินถูก

  ต่อวงจร:
    รีเลย์โมดูล IN -> GPIO 26, VCC -> 5V (VIN), GND -> GND
    DS3231 (ถ้ามี): SDA -> GPIO 21, SCL -> GPIO 22, VCC -> 3.3V, GND -> GND

  ⚠️ ฝั่งกริ่ง 220V (ขา COM / NO ของรีเลย์) ให้ช่างไฟเป็นคนต่อ
     ห้ามต่อไฟบ้านเข้าบอร์ด ESP32 เด็ดขาด

  ไลบรารีที่ต้องลง (ถ้าใช้ DS3231): RTClib by Adafruit
*/
#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include "secrets.h"   // คัดลอกจาก secrets.example.h แล้วแก้ค่า

// ---------- ตั้งค่า ----------
#define USE_DS3231 1             // ไม่มีโมดูลนาฬิกา ให้เปลี่ยนเป็น 0
const int  RELAY_PIN      = 26;
const bool RELAY_ACTIVE_LOW = true;   // รีเลย์โมดูลส่วนใหญ่ทำงานเมื่อขาเป็น LOW
const int  RING_SECONDS   = 5;

// ตารางคาบ (ชั่วโมง, นาที) แก้ได้ตามโรงเรียน
const uint8_t BELL_TIMES[][2] = {
  {8, 0}, {8, 30}, {9, 20}, {10, 10}, {11, 0}, {11, 50},
  {13, 0}, {13, 50}, {14, 40}, {15, 30}
};
const int N_BELLS = sizeof(BELL_TIMES) / sizeof(BELL_TIMES[0]);

// วันที่ให้ดัง (0=อาทิตย์ ... 6=เสาร์) ค่าเริ่มต้น จันทร์-ศุกร์
const bool RING_ON_DAY[7] = {false, true, true, true, true, true, false};

#if USE_DS3231
#include <RTClib.h>
RTC_DS3231 rtc;
bool rtcOk = false;
#endif

int lastRungMinute = -1;   // กันดังซ้ำในนาทีเดียวกัน

void relayOn(bool on) {
  digitalWrite(RELAY_PIN, (on ^ RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

void ringBell() {
  Serial.println("RING!");
  relayOn(true);
  delay(RING_SECONDS * 1000UL);
  relayOn(false);
}

bool ntpTime(struct tm &t) {
  if (!getLocalTime(&t, 2000)) return false;
  return t.tm_year > (2020 - 1900);
}

// อ่านเวลาปัจจุบัน: ใช้ NTP ก่อน ถ้าไม่ได้ ใช้ DS3231
bool nowTime(int &wday, int &hh, int &mm) {
  struct tm t;
  if (ntpTime(t)) {
    wday = t.tm_wday; hh = t.tm_hour; mm = t.tm_min;
#if USE_DS3231
    static unsigned long lastSync = 0;
    if (rtcOk && (lastSync == 0 || millis() - lastSync > 3600000UL)) {   // ซิงก์ DS3231 ทุกชั่วโมง
      rtc.adjust(DateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec));
      lastSync = millis();
    }
#endif
    return true;
  }
#if USE_DS3231
  if (rtcOk) {
    DateTime d = rtc.now();
    wday = d.dayOfTheWeek(); hh = d.hour(); mm = d.minute();
    return true;
  }
#endif
  return false;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(500);
  Serial.println(WiFi.status() == WL_CONNECTED ? "WiFi OK" : "WiFi failed (ใช้เวลาจาก DS3231 แทน)");
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  relayOn(false);
#if USE_DS3231
  Wire.begin(21, 22);
  rtcOk = rtc.begin();
  Serial.println(rtcOk ? "DS3231 OK" : "ไม่พบ DS3231");
#endif
  connectWiFi();
  configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");   // เวลาไทย UTC+7
}

void loop() {
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 30000) {
    lastWiFiCheck = millis();
    connectWiFi();
  }

  int wday, hh, mm;
  if (nowTime(wday, hh, mm)) {
    int key = hh * 60 + mm;
    if (key != lastRungMinute && RING_ON_DAY[wday]) {
      for (int i = 0; i < N_BELLS; i++) {
        if (BELL_TIMES[i][0] == hh && BELL_TIMES[i][1] == mm) {
          lastRungMinute = key;
          ringBell();
          break;
        }
      }
    }
  }
  delay(1000);
}
