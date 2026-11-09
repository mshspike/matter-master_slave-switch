// Pin map and timing. VERIFY every pin against the DFR1075 (FireBeetle 2 ESP32-C6)
// pinout before wiring: these are planning values, not checked on hardware.
#pragma once

// Wake-capable inputs. On the ESP32-C6 only GPIO0..GPIO7 (LP IO) can wake from deep sleep.
// Wire each input to GND through the switch, with an external 1 Mohm pull-up to 3V3.
constexpr int PIN_ROCKER  = 0;  // closed = slave mode
constexpr int PIN_SELECT  = 1;
constexpr int PIN_ROLE    = 2;
constexpr int PIN_SEND    = 3;
constexpr int PIN_REFRESH = 4;

// Waveshare 2.13in e-paper HAT V4 (SSD1680) on SPI.
constexpr int PIN_EPD_SCK  = 23;
constexpr int PIN_EPD_MOSI = 22;
constexpr int PIN_EPD_CS   = 18;
constexpr int PIN_EPD_DC   = 19;
constexpr int PIN_EPD_RST  = 20;
constexpr int PIN_EPD_BUSY = 21;

// I2C for the optional MAX17048 fuel gauge.
constexpr int PIN_SDA = 6;
constexpr int PIN_SCL = 7;

// Timing (ms).
constexpr uint32_t DEBOUNCE_MS        = 30;
constexpr uint32_t WIFI_TIMEOUT_MS    = 15000;
// Controllers resubscribe to a rebooted Matter node after it reappears. Events sent before
// that are lost, so wait before sending. Tune by testing with your hub.
constexpr uint32_t MATTER_SETTLE_MS   = 4000;
constexpr uint32_t AFTER_SEND_MS      = 1500;
constexpr uint32_t DECOMMISSION_HOLD_MS = 8000;  // hold REFRESH at wake
