#include <Arduino.h>

#include "ChickadeeConfig.h"
#include "Display.h"
#include "Menu.h"
#include "Radio.h"
#include "SpectrumScanner.h"

namespace {

enum class AppScreen : uint8_t {
  MainMenu,
  Spectrum,
  LiveRSSI,
  Placeholder
};

enum class SelectEvent : uint8_t {
  None,
  Pressed,
  Released
};

struct Button {
  uint8_t pin;
  bool stableState;
  bool previousReading;
  unsigned long lastChangeTime;
};

// General button timing
constexpr unsigned long DEBOUNCE_MS = 25;

// Live RSSI screen timing
constexpr unsigned long RSSI_UPDATE_MS = 100;

// Hold Select for this long to activate Auto Seek
constexpr unsigned long SEEK_HOLD_MS = 400;

// OLED update interval while Auto Seek is active
constexpr unsigned long SEEK_DISPLAY_UPDATE_MS = 80;

// Live RSSI frequency controls
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

/*
 * Dedicated Select-button state for the Spectrum screen.
 * This is separate from the normal buttonPressed() system
 * because Spectrum must distinguish a short press from a hold.
 */
bool spectrumSelectRawState = HIGH;
bool spectrumSelectStableState = HIGH;
bool spectrumSeekStarted = false;

unsigned long spectrumSelectChangedAt = 0;
unsigned long spectrumSelectPressedAt = 0;
unsigned long lastSeekDisplayUpdate = 0;

/*
 * Returns true once when a button becomes pressed.
 * A release does not return true.
 */
bool buttonPressed(Button& button) {
  const bool currentReading =
      digitalRead(button.pin);

  if (
      currentReading !=
      button.previousReading
  ) {
    button.lastChangeTime =
        millis();

    button.previousReading =
        currentReading;
  }

  if (
      millis() - button.lastChangeTime <
      DEBOUNCE_MS
  ) {
    return false;
  }

  if (
      currentReading ==
      button.stableState
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

/*
 * Dedicated debounced event detector for Select
 * while on the Spectrum screen.
 */
SelectEvent updateSpectrumSelect() {
  const bool rawState =
      digitalRead(
          Chickadee::BTN_SELECT
      );

  if (
      rawState !=
      spectrumSelectRawState
  ) {
    spectrumSelectRawState =
        rawState;

    spectrumSelectChangedAt =
        millis();
  }

  if (
      millis() -
          spectrumSelectChangedAt <
      DEBOUNCE_MS
  ) {
    return SelectEvent::None;
  }

  if (
      spectrumSelectStableState ==
      spectrumSelectRawState
  ) {
    return SelectEvent::None;
  }

  spectrumSelectStableState =
      spectrumSelectRawState;

  if (
      spectrumSelectStableState ==
      LOW
  ) {
    spectrumSelectPressedAt =
        millis();

    Serial.println(
        "Spectrum SELECT pressed."
    );

    return SelectEvent::Pressed;
  }

  Serial.println(
      "Spectrum SELECT released."
  );

  return SelectEvent::Released;
}

void drawMainMenu() {
  ChickadeeRadio::stopLiveRSSI();

  spectrumSeekStarted = false;

  spectrumSelectRawState =
      digitalRead(
          Chickadee::BTN_SELECT
      );

  spectrumSelectStableState =
      spectrumSelectRawState;

  spectrumSelectChangedAt =
      millis();

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

  Serial.print(
      rssi,
      1
  );

  Serial.println(" dBm");
}

void drawSpectrum() {
  ChickadeeDisplay::showSpectrum(
      ChickadeeSpectrum::
          getStartFrequency(),

      ChickadeeSpectrum::
          getEndFrequency(),

      ChickadeeSpectrum::
          getLiveTrace(),

      ChickadeeSpectrum::
          getPeakTrace(),

      ChickadeeSpectrum::POINT_COUNT,

      ChickadeeSpectrum::
          getPeakFrequency(),

      ChickadeeSpectrum::
          getPeakRSSI()
  );
}

void drawSpectrumSeek() {
  ChickadeeDisplay::showSpectrumSeek(
      ChickadeeSpectrum::
          getSeekFrequency(),

      ChickadeeSpectrum::
          getSeekRSSI(),

      ChickadeeSpectrum::
          getSeekProgress()
  );
}

void openSpectrum() {
  Serial.println(
      "Opening Spectrum Scan."
  );

  if (
      !radioReady ||
      !ChickadeeSpectrum::begin(
          430.0f
      )
  ) {
    currentScreen =
        AppScreen::Placeholder;

    ChickadeeDisplay::showComingSoon(
        "SCAN ERROR"
    );

    return;
  }

  /*
   * Initialize the dedicated Select detector
   * from the button's current physical state.
   */
  spectrumSelectRawState =
      digitalRead(
          Chickadee::BTN_SELECT
      );

  spectrumSelectStableState =
      spectrumSelectRawState;

  spectrumSelectChangedAt =
      millis();

  spectrumSelectPressedAt =
      millis();

  spectrumSeekStarted = false;
  lastSeekDisplayUpdate = 0;

  /*
   * Synchronize the normal Select Button object so
   * returning to the menu does not create a false press.
   */
  selectButton.stableState =
      spectrumSelectRawState;

  selectButton.previousReading =
      spectrumSelectRawState;

  selectButton.lastChangeTime =
      millis();

  currentScreen =
      AppScreen::Spectrum;

  drawSpectrum();
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

  if (
      !ChickadeeRadio::
          startLiveRSSI()
  ) {
    Serial.print(
        "Could not start Live RSSI. Error: "
    );

    Serial.println(
        ChickadeeRadio::
            getLastError()
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
      ChickadeeMenu::
          getSelectedItem();

  if (
      selected ==
      ChickadeeMenu::Item::
          SpectrumScan
  ) {
    openSpectrum();
    return;
  }

  if (
      selected ==
      ChickadeeMenu::Item::
          LiveRSSI
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
  if (
      buttonPressed(upButton)
  ) {
    ChickadeeMenu::moveUp();

    ChickadeeDisplay::showMainMenu(
        ChickadeeMenu::
            getSelectedIndex()
    );
  }

  if (
      buttonPressed(downButton)
  ) {
    ChickadeeMenu::moveDown();

    ChickadeeDisplay::showMainMenu(
        ChickadeeMenu::
            getSelectedIndex()
    );
  }

  if (
      buttonPressed(selectButton)
  ) {
    openSelectedFeature();
  }

  // Back has no action on the main menu.
  buttonPressed(backButton);
}

void handleSpectrum() {
  /*
   * Advances either:
   * - the normal graph sweep,
   * - the coarse Auto Seek,
   * - or the Auto Seek refinement scan.
   */
  ChickadeeSpectrum::update();

  const SelectEvent selectEvent =
      updateSpectrumSelect();

  /*
   * Start Auto Seek after Select remains
   * pressed longer than SEEK_HOLD_MS.
   */
  if (
      spectrumSelectStableState ==
          LOW &&
      !spectrumSeekStarted &&
      millis() -
          spectrumSelectPressedAt >=
          SEEK_HOLD_MS
  ) {
    spectrumSeekStarted = true;

    ChickadeeSpectrum::startSeek();

    lastSeekDisplayUpdate =
        0;

    Serial.println(
        "AUTO SEEK STARTED"
    );

    drawSpectrumSeek();
  }

  /*
   * Handle releasing Select.
   */
  if (
      selectEvent ==
      SelectEvent::Released
  ) {
    if (spectrumSeekStarted) {
      const float seekFrequency =
          ChickadeeSpectrum::
              getSeekFrequency();

      const float seekRSSI =
          ChickadeeSpectrum::
              getSeekRSSI();

      Serial.print(
          "AUTO SEEK RELEASED: "
      );

      Serial.print(
          seekFrequency,
          3
      );

      Serial.print(" MHz, ");

      Serial.print(
          seekRSSI,
          1
      );

      Serial.println(" dBm");

      /*
       * Return to graph mode and center
       * the 7 MHz window on the detected signal.
       */
      ChickadeeSpectrum::
          stopSeekAndCenter();

      spectrumSeekStarted =
          false;

      drawSpectrum();
    } else {
      /*
       * A short Select press clears the
       * retained graph peak.
       */
      ChickadeeSpectrum::
          clearPeaks();

      Serial.println(
          "Spectrum peak cleared."
      );

      drawSpectrum();
    }
  }

  /*
   * Auto Seek is active while Select remains held.
   */
  if (spectrumSeekStarted) {
    if (
        millis() -
            lastSeekDisplayUpdate >=
        SEEK_DISPLAY_UPDATE_MS
    ) {
      lastSeekDisplayUpdate =
          millis();

      drawSpectrumSeek();
    }

    /*
     * Back cancels seeking and returns
     * to the normal graph.
     */
    if (
        buttonPressed(backButton)
    ) {
      ChickadeeSpectrum::
          cancelSeek();

      spectrumSeekStarted =
          false;

      spectrumSelectRawState =
          digitalRead(
              Chickadee::BTN_SELECT
          );

      spectrumSelectStableState =
          spectrumSelectRawState;

      spectrumSelectChangedAt =
          millis();

      Serial.println(
          "AUTO SEEK CANCELLED"
      );

      drawSpectrum();
    }

    // Up and Down are ignored while seeking.
    buttonPressed(upButton);
    buttonPressed(downButton);

    return;
  }

  /*
   * Normal Spectrum controls.
   */
  if (
      buttonPressed(upButton)
  ) {
    ChickadeeSpectrum::panUp();

    Serial.print(
        "Spectrum window: "
    );

    Serial.print(
        ChickadeeSpectrum::
            getStartFrequency(),
        1
    );

    Serial.print("-");

    Serial.print(
        ChickadeeSpectrum::
            getEndFrequency(),
        1
    );

    Serial.println(" MHz");

    drawSpectrum();
  }

  if (
      buttonPressed(downButton)
  ) {
    ChickadeeSpectrum::panDown();

    Serial.print(
        "Spectrum window: "
    );

    Serial.print(
        ChickadeeSpectrum::
            getStartFrequency(),
        1
    );

    Serial.print("-");

    Serial.print(
        ChickadeeSpectrum::
            getEndFrequency(),
        1
    );

    Serial.println(" MHz");

    drawSpectrum();
  }

  if (
      buttonPressed(backButton)
  ) {
    Serial.println(
        "Leaving Spectrum Scan."
    );

    /*
     * Synchronize the regular Select-button object
     * before returning to the normal menu controls.
     */
    selectButton.stableState =
        spectrumSelectStableState;

    selectButton.previousReading =
        spectrumSelectRawState;

    selectButton.lastChangeTime =
        millis();

    drawMainMenu();
    return;
  }

  /*
   * Redraw after each completed graph sweep.
   */
  if (
      ChickadeeSpectrum::
          hasNewSweep()
  ) {
    drawSpectrum();
  }
}

void handleLiveRSSI() {
  if (
      buttonPressed(upButton)
  ) {
    float frequency =
        ChickadeeRadio::
            getFrequency() +
        FREQUENCY_STEP_MHZ;

    if (
        frequency >
        MAX_FREQUENCY_MHZ
    ) {
      frequency =
          MIN_FREQUENCY_MHZ;
    }

    if (
        !ChickadeeRadio::
            setFrequency(
                frequency
            )
    ) {
      Serial.print(
          "Frequency increase failed. Error: "
      );

      Serial.println(
          ChickadeeRadio::
              getLastError()
      );
    }

    drawLiveRSSI();
  }

  if (
      buttonPressed(downButton)
  ) {
    float frequency =
        ChickadeeRadio::
            getFrequency() -
        FREQUENCY_STEP_MHZ;

    if (
        frequency <
        MIN_FREQUENCY_MHZ
    ) {
      frequency =
          MAX_FREQUENCY_MHZ;
    }

    if (
        !ChickadeeRadio::
            setFrequency(
                frequency
            )
    ) {
      Serial.print(
          "Frequency decrease failed. Error: "
      );

      Serial.println(
          ChickadeeRadio::
              getLastError()
      );
    }

    drawLiveRSSI();
  }

  // Select currently has no action in Live RSSI.
  buttonPressed(selectButton);

  if (
      buttonPressed(backButton)
  ) {
    Serial.println(
        "Leaving Live RSSI."
    );

    drawMainMenu();
    return;
  }

  if (
      millis() -
          lastRSSIUpdate >=
      RSSI_UPDATE_MS
  ) {
    lastRSSIUpdate =
        millis();

    drawLiveRSSI();
  }
}

void handlePlaceholder() {
  buttonPressed(upButton);
  buttonPressed(downButton);
  buttonPressed(selectButton);

  if (
      buttonPressed(backButton)
  ) {
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

  Serial.println(
      "============================"
  );

  Serial.println(
      "Chickadee starting..."
  );

  Serial.print(
      "Firmware version: "
  );

  Serial.println(
      Chickadee::VERSION
  );

  Serial.println(
      "============================"
  );

  oledReady =
      ChickadeeDisplay::begin();

  if (oledReady) {
    Serial.println(
        "OLED initialization successful."
    );

    ChickadeeDisplay::
        showBootScreen();
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

    Serial.print(
        "Listening at "
    );

    Serial.print(
        ChickadeeRadio::
            getFrequency(),
        3
    );

    Serial.println(" MHz");
  } else {
    Serial.print(
        "CC1101 initialization FAILED: "
    );

    Serial.println(
        ChickadeeRadio::
            getLastError()
    );
  }

  if (oledReady) {
    ChickadeeDisplay::
        showHardwareStatus(
            oledReady,
            radioReady
        );

    delay(1500);

    ChickadeeMenu::begin();

    drawMainMenu();
  }

  Serial.println(
      "Chickadee ready."
  );
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

    case AppScreen::Spectrum:
      handleSpectrum();
      break;

    case AppScreen::LiveRSSI:
      handleLiveRSSI();
      break;

    case AppScreen::Placeholder:
      handlePlaceholder();
      break;
  }
}