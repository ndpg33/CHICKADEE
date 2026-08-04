#include <Arduino.h>

#include "ChickadeeConfig.h"
#include "Display.h"
#include "Menu.h"
#include "Radio.h"

namespace {

enum class AppScreen : uint8_t {
  MainMenu,
  FeaturePage
};

struct Button {
  uint8_t pin;
  bool stableState;
  bool previousReading;
  unsigned long lastChangeTime;
};

constexpr unsigned long DEBOUNCE_MS = 25;

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

AppScreen currentScreen = AppScreen::MainMenu;
bool oledReady = false;
bool radioReady = false;

/*
 * Returns true once when a button becomes pressed.
 * Releasing the button does not produce an event.
 */
bool buttonPressed(Button& button) {
  const bool currentReading =
      digitalRead(button.pin);

  if (currentReading != button.previousReading) {
    button.lastChangeTime = millis();
    button.previousReading = currentReading;
  }

  if (
      millis() - button.lastChangeTime
      < DEBOUNCE_MS
  ) {
    return false;
  }

  if (currentReading == button.stableState) {
    return false;
  }

  button.stableState = currentReading;

  return button.stableState == LOW;
}

void initializeButton(Button& button) {
  pinMode(button.pin, INPUT_PULLUP);

  const bool initialState =
      digitalRead(button.pin);

  button.stableState = initialState;
  button.previousReading = initialState;
  button.lastChangeTime = millis();
}

void drawMainMenu() {
  currentScreen = AppScreen::MainMenu;

  ChickadeeDisplay::showMainMenu(
      ChickadeeMenu::getSelectedIndex()
  );
}

void openSelectedFeature() {
  const ChickadeeMenu::Item selectedItem =
      ChickadeeMenu::getSelectedItem();

  const char* featureName =
      ChickadeeMenu::getItemLabel(selectedItem);

  Serial.print("Opening: ");
  Serial.println(featureName);

  currentScreen = AppScreen::FeaturePage;

  ChickadeeDisplay::showComingSoon(
      featureName
  );
}

void handleMainMenu() {
  if (buttonPressed(upButton)) {
    ChickadeeMenu::moveUp();

    Serial.print("Selected: ");
    Serial.println(
        ChickadeeMenu::getItemLabel(
            ChickadeeMenu::getSelectedItem()
        )
    );

    drawMainMenu();
  }

  if (buttonPressed(downButton)) {
    ChickadeeMenu::moveDown();

    Serial.print("Selected: ");
    Serial.println(
        ChickadeeMenu::getItemLabel(
            ChickadeeMenu::getSelectedItem()
        )
    );

    drawMainMenu();
  }

  if (buttonPressed(selectButton)) {
    openSelectedFeature();
  }

  // Consume BACK presses while already on the menu.
  buttonPressed(backButton);
}

void handleFeaturePage() {
  /*
   * Consume events from buttons that currently
   * have no function on placeholder pages.
   */
  buttonPressed(upButton);
  buttonPressed(downButton);
  buttonPressed(selectButton);

  if (buttonPressed(backButton)) {
    Serial.println("Returning to main menu.");
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

  oledReady = ChickadeeDisplay::begin();

  if (!oledReady) {
    Serial.println(
        "OLED initialization FAILED."
    );
  } else {
    Serial.println(
        "OLED initialization successful."
    );

    ChickadeeDisplay::showBootScreen();
  }

  delay(1000);

  Serial.println("Initializing CC1101...");

  radioReady = ChickadeeRadio::begin();

  if (radioReady) {
    Serial.println(
        "CC1101 initialization successful."
    );

    Serial.print("Listening at ");
    Serial.print(
        ChickadeeRadio::getFrequency(),
        3
    );
    Serial.println(" MHz");
  } else {
    Serial.print(
        "CC1101 initialization FAILED. Error: "
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

    case AppScreen::FeaturePage:
      handleFeaturePage();
      break;
  }
}