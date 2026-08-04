#include "Radio.h"

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

#include "ChickadeeConfig.h"

namespace {

constexpr float DEFAULT_FREQUENCY_MHZ = 433.92;

// Module(CS, GDO0, RESET, GDO2)
CC1101 radio = new Module(
    Chickadee::CC1101_CS,
    Chickadee::CC1101_GDO0,
    RADIOLIB_NC,
    Chickadee::CC1101_GDO2
);

bool radioReady = false;
bool liveRSSIMode = false;

int lastError = RADIOLIB_ERR_NONE;

float currentFrequencyMHz =
    DEFAULT_FREQUENCY_MHZ;

}  // namespace

namespace ChickadeeRadio {

bool begin() {
  SPI.begin(
      Chickadee::CC1101_SCK,
      Chickadee::CC1101_MISO,
      Chickadee::CC1101_MOSI,
      Chickadee::CC1101_CS
  );

  lastError = radio.begin(
      DEFAULT_FREQUENCY_MHZ,
      4.8,    // Bitrate in kbps
      5.0,    // Frequency deviation in kHz
      203.0,  // Receiver bandwidth in kHz
      10,     // TX power in dBm
      16      // Preamble length in bits
  );

  radioReady =
      lastError == RADIOLIB_ERR_NONE;

  if (!radioReady) {
    return false;
  }

  currentFrequencyMHz =
      DEFAULT_FREQUENCY_MHZ;

  lastError = radio.startReceive();

  radioReady =
      lastError == RADIOLIB_ERR_NONE;

  return radioReady;
}

bool isReady() {
  return radioReady;
}

int getLastError() {
  return lastError;
}

float getFrequency() {
  return currentFrequencyMHz;
}

bool setFrequency(float frequencyMHz) {
  if (!radioReady) {
    return false;
  }

  lastError =
      radio.setFrequency(frequencyMHz);

  if (lastError != RADIOLIB_ERR_NONE) {
    return false;
  }

  currentFrequencyMHz = frequencyMHz;

  /*
   * Retuning may disturb the current receive state,
   * so restart the appropriate receive mode.
   */
  if (liveRSSIMode) {
    lastError =
        radio.receiveDirectAsync();
  } else {
    lastError =
        radio.startReceive();
  }

  return lastError == RADIOLIB_ERR_NONE;
}

bool startLiveRSSI() {
  if (!radioReady) {
    return false;
  }

  lastError =
      radio.receiveDirectAsync();

  liveRSSIMode =
      lastError == RADIOLIB_ERR_NONE;

  return liveRSSIMode;
}

void stopLiveRSSI() {
  if (!radioReady) {
    return;
  }

  liveRSSIMode = false;

  radio.standby();

  lastError =
      radio.startReceive();
}

float readRSSI() {
  if (!radioReady || !liveRSSIMode) {
    return -120.0f;
  }

  return radio.getRSSI();
}

}  // namespace ChickadeeRadio