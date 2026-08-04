#include <Arduino.h>

#include "ChickadeeConfig.h"
#include "Display.h"

namespace {

bool lastUpState = HIGH;
bool lastDownState = HIGH;
bool lastSelectState = HIGH;
bool lastBackState = HIGH;

constexpr unsigned long DEBOUNCE_MS = 25;

void checkButton(
    uint8_t pin,
    bool& previousState,
    const char* buttonName
) {
  bool currentState = digitalRead(pin);

  if (currentState == previousState) {
    return;
  }

  delay(DEBOUNCE_MS);
  currentState = digitalRead(pin);

  if (currentState == previousState) {
    return;
  }

  previousState = currentState;
  bool pressed = currentState == LOW;

  Serial.print(buttonName);
  Serial.println(pressed ? " PRESSED" : " RELEASED");

  ChickadeeDisplay::showButtonEvent(
      buttonName,
      pressed
  );
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(Chickadee::BTN_UP, INPUT_PULLUP);
  pinMode(Chickadee::BTN_DOWN, INPUT_PULLUP);
  pinMode(Chickadee::BTN_SELECT, INPUT_PULLUP);
  pinMode(Chickadee::BTN_BACK, INPUT_PULLUP);

  Serial.println();
  Serial.println("============================");
  Serial.println("Chickadee starting...");
  Serial.print("Firmware version: ");
  Serial.println(Chickadee::VERSION);
  Serial.println("============================");

  bool oledReady = ChickadeeDisplay::begin();

  if (!oledReady) {
    Serial.println("OLED initialization FAILED.");
  } else {
    Serial.println("OLED initialization successful.");

    ChickadeeDisplay::showBootScreen();
    delay(1500);

    ChickadeeDisplay::showHardwareStatus(
        true,
        false
    );
  }

  lastUpState = digitalRead(Chickadee::BTN_UP);
  lastDownState = digitalRead(Chickadee::BTN_DOWN);
  lastSelectState = digitalRead(Chickadee::BTN_SELECT);
  lastBackState = digitalRead(Chickadee::BTN_BACK);
}

void loop() {
  checkButton(
      Chickadee::BTN_UP,
      lastUpState,
      "UP"
  );

  checkButton(
      Chickadee::BTN_DOWN,
      lastDownState,
      "DOWN"
  );

  checkButton(
      Chickadee::BTN_SELECT,
      lastSelectState,
      "SELECT"
  );

  checkButton(
      Chickadee::BTN_BACK,
      lastBackState,
      "BACK"
  );
}