#include "Display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#include "ChickadeeConfig.h"

namespace {

Adafruit_SSD1306 display(
    Chickadee::OLED_WIDTH,
    Chickadee::OLED_HEIGHT,
    &Wire,
    -1
);

void prepareText(uint8_t size = 1) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(size);
  display.setCursor(0, 0);
}

}  // namespace

namespace ChickadeeDisplay {

bool begin() {
  Wire.begin(
      Chickadee::OLED_SDA,
      Chickadee::OLED_SCL
  );

  return display.begin(
      SSD1306_SWITCHCAPVCC,
      Chickadee::OLED_ADDRESS
  );
}

void showBootScreen() {
  prepareText(2);

  display.println("CHICKADEE");

  display.setTextSize(1);
  display.println();
  display.print("Firmware v");
  display.println(Chickadee::VERSION);
  display.println();
  display.println("Initializing...");

  display.display();
}

void showHardwareStatus(bool oledReady, bool radioReady) {
  prepareText(1);

  display.println("CHICKADEE STATUS");
  display.println("----------------");

  display.print("OLED:    ");
  display.println(oledReady ? "OK" : "FAILED");

  display.print("CC1101:  ");
  display.println(radioReady ? "OK" : "NOT TESTED");

  display.println();
  display.println("Buttons ready");

  display.display();
}

void showButtonEvent(const char* buttonName, bool pressed) {
  prepareText(2);

  display.println(buttonName);

  display.setTextSize(1);
  display.println();
  display.println(pressed ? "PRESSED" : "RELEASED");

  display.display();
}

}  // namespace ChickadeeDisplay