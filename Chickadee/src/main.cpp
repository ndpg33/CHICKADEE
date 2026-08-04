#include <Arduino.h>

#include "ChickadeeConfig.h"
#include "Display.h"
#include "Menu.h"
#include "Radio.h"

namespace {

enum class AppScreen : uint8_t {
  MainMenu,
  LiveRSSI,
  Placeholder
};

struct Button {
  uint8_t pin;
  bool stableState;
  bool previousReading;
  unsigned long lastChangeTime;
};

constexpr unsigned long DEBOUNCE_MS = 25;
constexpr unsigned long RSSI_UPDATE_MS = 100;

constexpr float FREQUENCY_STEP_MHZ = 0.05f;
constexpr float MIN_FREQUENCY_MHZ = 387.0f;
constexpr float MAX_FREQUENCY_MHZ = 464.0f;

Button upButton = {
    Chickadee::BTN_UP,
    HIGH,
    HIGH,
    0
};

Button downButton = {
    Chickadee::BTN_DOWN,
    HIGH,
    HIGH,
    0
};

Button selectButton = {
    Chickadee::BTN_SELECT,
    HIGH,
    HIGH,
    0
};

Button backButton = {
    Chickadee::BTN_BACK,
    HIGH,
    HIGH,
    0
};

AppScreen currentScreen =
    AppScreen::MainMenu;

bool oledReady = false;
bool radioReady = false;

unsigned long lastRSSIUpdate = 0;

bool buttonPressed(Button& button) {
  const bool currentReading =
      digitalRead(button.pin);

  if (
      currentReading
      != button.previousReading
  ) {
    button.lastChangeTime = millis();
    button.previousReading =
        currentReading;
  }

  if (
      millis() - button.lastChangeTime
      < DEBOUNCE_MS
  ) {
    return false;
  }

  if (
      currentReading
      == button.stableState
  ) {
    return false;
  }

  button.stableState =
      currentReading;

  return button.stableState == LOW;
}

void initializeButton(Button& button) {
  pinMode(
      button.pin,
      INPUT_PULLUP
  );

  const bool initialState =
      digitalRead(button.pin);

  button.stableState =
      initialState;

  button.previousReading =
      initialState;

  button.lastChangeTime =
      millis();
}

void drawMainMenu() {
  ChickadeeRadio::stopLiveRSSI();

  currentScreen =
      AppScreen::MainMenu;

  ChickadeeDisplay::showMainMenu(
      ChickadeeMenu::getSelectedIndex()
  );
}

void drawLiveRSSI() {
  const float rssi =
      ChickadeeRadio::readRSSI();

  ChickadeeDisplay::showLiveRSSI(
      ChickadeeRadio::getFrequency(),
      rssi
  );

  Serial.print("Frequency: ");
  Serial.print(
      ChickadeeRadio::getFrequency(),
      3
  );
  Serial.print(" MHz, RSSI: ");
  Serial.print(rssi, 1);
  Serial.println(" dBm");
}

void openLiveRSSI() {
  Serial.println(
      "Opening Live RSSI."
  );

  if (!radioReady) {
    currentScreen =
        AppScreen::Placeholder;

    ChickadeeDisplay::showComingSoon(
        "RADIO ERROR"
    );

    return;
  }

  if (!ChickadeeRadio::startLiveRSSI()) {
    Serial.print(
        "Could not start live RSSI. Error: "
    );

    Serial.println(
        ChickadeeRadio::getLastError()
    );

    currentScreen =
        AppScreen::Placeholder;

    ChickadeeDisplay::showComingSoon(
        "RSSI ERROR"
    );

    return;
  }

  currentScreen =
      AppScreen::LiveRSSI;

  lastRSSIUpdate = 0;
  drawLiveRSSI();
}

void openSelectedFeature() {
  const ChickadeeMenu::Item selected =
      ChickadeeMenu::getSelectedItem();

  if (
      selected
      == ChickadeeMenu::Item::LiveRSSI
  ) {
    openLiveRSSI();
    return;
  }

  currentScreen =
      AppScreen::Placeholder;

  ChickadeeDisplay::showComingSoon(
      ChickadeeMenu::getItemLabel(
          selected
      )
  );
}

void handleMainMenu() {
  if (buttonPressed(upButton)) {
    ChickadeeMenu::moveUp();

    ChickadeeDisplay::showMainMenu(
        ChickadeeMenu::getSelectedIndex()
    );
  }

  if (buttonPressed(downButton)) {
    ChickadeeMenu::moveDown();

    ChickadeeDisplay::showMainMenu(
        ChickadeeMenu::getSelectedIndex()
    );
  }

  if (buttonPressed(selectButton)) {
    openSelectedFeature();
  }

  buttonPressed(backButton);
}

void handleLiveRSSI() {
  if (buttonPressed(upButton)) {
    float frequency =
        ChickadeeRadio::getFrequency()
        + FREQUENCY_STEP_MHZ;

    if (frequency > MAX_FREQUENCY_MHZ) {
      frequency = MIN_FREQUENCY_MHZ;
    }

    ChickadeeRadio::setFrequency(
        frequency
    );

    drawLiveRSSI();
  }

  if (buttonPressed(downButton)) {
    float frequency =
        ChickadeeRadio::getFrequency()
        - FREQUENCY_STEP_MHZ;

    if (frequency < MIN_FREQUENCY_MHZ) {
      frequency = MAX_FREQUENCY_MHZ;
    }

    ChickadeeRadio::setFrequency(
        frequency
    );

    drawLiveRSSI();
  }

  buttonPressed(selectButton);

  if (buttonPressed(backButton)) {
    Serial.println(
        "Leaving Live RSSI."
    );

    drawMainMenu();
    return;
  }

  if (
      millis() - lastRSSIUpdate
      >= RSSI_UPDATE_MS
  ) {
    lastRSSIUpdate = millis();
    drawLiveRSSI();
  }
}

void handlePlaceholder() {
  buttonPressed(upButton);
  buttonPressed(downButton);
  buttonPressed(selectButton);

  if (buttonPressed(backButton)) {
    drawMainMenu();
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  initializeButton(upButton);
  initializeButton(downButton);
  initializeButton(selectButton);
  initializeButton(backButton);

  Serial.println();
  Serial.println("============================");
  Serial.println("Chickadee starting...");
  Serial.print("Firmware version: ");
  Serial.println(Chickadee::VERSION);
  Serial.println("============================");

  oledReady =
      ChickadeeDisplay::begin();

  if (oledReady) {
    Serial.println(
        "OLED initialization successful."
    );

    ChickadeeDisplay::showBootScreen();
  } else {
    Serial.println(
        "OLED initialization FAILED."
    );
  }

  delay(1000);

  Serial.println(
      "Initializing CC1101..."
  );

  radioReady =
      ChickadeeRadio::begin();

  if (radioReady) {
    Serial.println(
        "CC1101 initialization successful."
    );
  } else {
    Serial.print(
        "CC1101 initialization FAILED: "
    );

    Serial.println(
        ChickadeeRadio::getLastError()
    );
  }

  if (oledReady) {
    ChickadeeDisplay::showHardwareStatus(
        oledReady,
        radioReady
    );

    delay(1500);

    ChickadeeMenu::begin();
    drawMainMenu();
  }

  Serial.println("Chickadee ready.");
}

void loop() {
  if (!oledReady) {
    delay(100);
    return;
  }

  switch (currentScreen) {
    case AppScreen::MainMenu:
      handleMainMenu();
      break;

    case AppScreen::LiveRSSI:
      handleLiveRSSI();
      break;

    case AppScreen::Placeholder:
      handlePlaceholder();
      break;
  }
}