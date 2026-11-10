// ESP32 Matter switch: battery-powered master/slave on/off controller for a Matter power strip.
// Hub route: the device exposes one Matter generic-switch endpoint per outlet and per state
// (8 endpoints). Create hub automations: "Outlet N ON pressed" -> turn strip outlet N on, and
// "Outlet N OFF pressed" -> turn it off. See README.
// Status: not yet compiled or run on hardware.
#include <Arduino.h>
#include <Matter.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

#include <Adafruit_MAX1704X.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <GxEPD2_BW.h>
#include <SPI.h>
#include <Wire.h>

#include "config.h"
#include "secrets.h"
#include "switch_logic.h"

using namespace sw;

static GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> epd(
    GxEPD2_213_B74(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
static Adafruit_MAX17048 gauge;
static Preferences prefs;
static Logic logic;
static MatterGenericSwitch endpoints[kOutlets * 2];  // [outlet*2] = ON, [outlet*2+1] = OFF

// ---------- persistence ----------
static void loadState() {
  prefs.begin("sw", true);
  logic.mode = static_cast<Mode>(prefs.getUChar("mode", 0));
  logic.selected = prefs.getUChar("sel", 0) % kOutlets;
  for (int i = 0; i < kOutlets; i++) {
    char k[8];
    snprintf(k, sizeof k, "r%d", i);
    logic.role[i] = static_cast<Role>(prefs.getUChar(k, static_cast<uint8_t>(logic.role[i])) % 3);
    snprintf(k, sizeof k, "o%d", i);
    logic.on[i] = prefs.getBool(k, false);
  }
  prefs.end();
}

static void saveState() {
  prefs.begin("sw", false);
  prefs.putUChar("mode", static_cast<uint8_t>(logic.mode));
  prefs.putUChar("sel", logic.selected);
  for (int i = 0; i < kOutlets; i++) {
    char k[8];
    snprintf(k, sizeof k, "r%d", i);
    prefs.putUChar(k, static_cast<uint8_t>(logic.role[i]));
    snprintf(k, sizeof k, "o%d", i);
    prefs.putBool(k, logic.on[i]);
  }
  prefs.end();
}

// ---------- inputs ----------
static bool pressed(int pin) { return digitalRead(pin) == LOW; }

static void setupInputs() {
  // External 1 Mohm pull-ups: the internal ones leak far more in deep sleep.
  for (int p : {PIN_MODE, PIN_SELECT, PIN_ROLE, PIN_SEND, PIN_REFRESH}) pinMode(p, INPUT);
}

static bool debounced(int pin) {
  if (!pressed(pin)) return false;
  delay(DEBOUNCE_MS);
  return pressed(pin);
}

// ---------- display ----------
static int batteryPercent() {
  if (!gauge.begin()) return -1;
  float p = gauge.cellPercent();
  return p < 0 ? -1 : (p > 100 ? 100 : static_cast<int>(p + 0.5f));
}

static const char* roleLabel(Role r) { return r == Role::Master ? "M" : (r == Role::Slave ? "S" : "-"); }

static void draw(int battery) {
  epd.setRotation(1);  // landscape 250x122
  epd.setFullWindow();
  epd.firstPage();
  do {
    epd.fillScreen(GxEPD_WHITE);
    epd.setTextColor(GxEPD_BLACK);
    epd.setFont(&FreeMonoBold9pt7b);
    epd.setCursor(4, 16);
    epd.print(logic.mode == Mode::Slave ? "MODE: SLAVE" : "MODE: NORMAL");
    epd.setCursor(190, 16);
    if (battery >= 0) epd.printf("%3d%%", battery); else epd.print(" --%");
    epd.drawFastHLine(0, 22, 250, GxEPD_BLACK);

    const int w = 58, h = 70, gap = 4, y = 30;
    for (int i = 0; i < kOutlets; i++) {
      int x = 4 + i * (w + gap);
      if (i == logic.selected) epd.drawRect(x - 2, y - 2, w + 4, h + 4, GxEPD_BLACK);
      epd.drawRect(x, y, w, h, GxEPD_BLACK);
      if (logic.on[i]) epd.fillRect(x, y + h - 22, w, 22, GxEPD_BLACK);
      epd.setFont(&FreeMonoBold9pt7b);
      epd.setCursor(x + 4, y + 16);
      epd.printf("#%d", i + 1);
      epd.setCursor(x + 4, y + 38);
      epd.print(logic.role[i] == Role::Master ? "MASTER" : (logic.role[i] == Role::Slave ? "SLAVE" : "-"));
      epd.setTextColor(logic.on[i] ? GxEPD_WHITE : GxEPD_BLACK);
      epd.setCursor(x + 4, y + h - 6);
      epd.print(logic.on[i] ? "ON" : "OFF");
      epd.setTextColor(GxEPD_BLACK);
    }
    epd.setFont(&FreeMono9pt7b);
    epd.setCursor(4, 118);
    epd.print("SEL ROLE SEND REFR");
  } while (epd.nextPage());
  epd.hibernate();  // e-paper keeps the image with no power
}

// ---------- Matter ----------
static bool matterStarted = false;

static void startRadio() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (auto& e : endpoints) e.begin();
  Matter.begin();
  matterStarted = true;
}

static bool waitOnline() {
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) delay(50);
  return WiFi.status() == WL_CONNECTED && Matter.isDeviceCommissioned();
}

static void sendCommands(const Result& r) {
  for (uint8_t i = 0; i < r.count; i++) {
    endpoints[r.cmds[i].outlet * 2 + (r.cmds[i].on ? 0 : 1)].click();
    delay(150);
  }
}

// ---------- sleep ----------
static void goToSleep() {
  const gpio_num_t clicks[] = {(gpio_num_t)PIN_SELECT, (gpio_num_t)PIN_ROLE, (gpio_num_t)PIN_SEND,
                               (gpio_num_t)PIN_REFRESH};
  uint64_t clickMask = 0;
  for (auto p : clicks) clickMask |= 1ULL << p;
  // Buttons wake on LOW (pressed). The mode slide switch wakes on the level opposite its current one.
  esp_deep_sleep_enable_gpio_wakeup(clickMask, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_MODE,
                                    pressed(PIN_MODE) ? ESP_GPIO_WAKEUP_GPIO_HIGH : ESP_GPIO_WAKEUP_GPIO_LOW);
  WiFi.mode(WIFI_OFF);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  setupInputs();
  loadState();
  Wire.begin(PIN_SDA, PIN_SCL);
  SPI.begin(PIN_EPD_SCK, -1, PIN_EPD_MOSI, PIN_EPD_CS);
  epd.init(0, true, 2, false);

  // Decommission: hold REFRESH for DECOMMISSION_HOLD_MS right after wake.
  if (debounced(PIN_REFRESH)) {
    uint32_t t0 = millis();
    while (pressed(PIN_REFRESH) && millis() - t0 < DECOMMISSION_HOLD_MS) delay(20);
    if (pressed(PIN_REFRESH)) {
      startRadio();
      Matter.decommission();
      Serial.println("Decommissioned. Pair again with the QR or manual code below.");
    }
  }

  Result total;
  // First power-up, including turning the power slide switch back on: always redraw.
  bool redraw = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED;

  delay(DEBOUNCE_MS);  // let the slide switch contacts settle if it caused the wake
  Result r = logic.setMode(pressed(PIN_MODE) ? Mode::Slave : Mode::Normal);
  redraw |= r.redraw;

  const struct { int pin; Input in; } buttons[] = {
      {PIN_SELECT, Input::Select}, {PIN_ROLE, Input::Role}, {PIN_SEND, Input::Send}, {PIN_REFRESH, Input::Refresh}};
  for (auto& b : buttons) {
    if (!debounced(b.pin)) continue;
    r = logic.press(b.in);
    redraw |= r.redraw;
    for (uint8_t i = 0; i < r.count; i++) total.cmds[total.count++] = r.cmds[i];
  }
  if (total.count > kOutlets) total.count = kOutlets;  // at most one Send per wake
  saveState();

  if (total.count > 0) startRadio();          // connect while the display refreshes
  if (redraw) draw(batteryPercent());
  if (total.count > 0) {
    if (waitOnline()) {
      delay(MATTER_SETTLE_MS);
      sendCommands(total);
      delay(AFTER_SEND_MS);
    } else {
      Serial.println("Matter/Wi-Fi not ready: command not sent");
    }
  }
  while (pressed(PIN_SELECT) || pressed(PIN_ROLE) || pressed(PIN_SEND) || pressed(PIN_REFRESH)) delay(20);
  goToSleep();
}

void loop() {}
