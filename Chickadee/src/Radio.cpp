#include "Radio.h"

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

#include "ChickadeeConfig.h"

namespace {

constexpr float DEFAULT_FREQUENCY_MHZ =
    433.92f;

constexpr uint8_t CC1101_RSSI_REGISTER =
    0x34;

constexpr uint8_t CC1101_STATUS_READ =
    0xC0;

constexpr float RSSI_OFFSET_DB =
    74.0f;

SPISettings cc1101SPISettings(
    4000000,
    MSBFIRST,
    SPI_MODE0
);

// Module(CS, GDO0, RESET, GDO2)
CC1101 radio = new Module(
    Chickadee::CC1101_CS,
    Chickadee::CC1101_GDO0,
    RADIOLIB_NC,
    Chickadee::CC1101_GDO2
);

bool radioReady = false;
bool directModeActive = false;

int lastError = RADIOLIB_ERR_NONE;

float currentFrequencyMHz =
    DEFAULT_FREQUENCY_MHZ;

uint8_t readStatusRegister(
    uint8_t address
) {
  SPI.beginTransaction(
      cc1101SPISettings
  );

  digitalWrite(
      Chickadee::CC1101_CS,
      LOW
  );

  /*
   * MISO goes LOW when the CC1101 crystal
   * oscillator is ready for SPI access.
   */
  const uint32_t timeoutStart = micros();

  while (
      digitalRead(
          Chickadee::CC1101_MISO
      ) == HIGH
  ) {
    if (
        micros() - timeoutStart
        > 1000
    ) {
      break;
    }
  }

  SPI.transfer(
      address | CC1101_STATUS_READ
  );

  const uint8_t value =
      SPI.transfer(0x00);

  digitalWrite(
      Chickadee::CC1101_CS,
      HIGH
  );

  SPI.endTransaction();

  return value;
}

float convertRawRSSI(uint8_t rawRSSI) {
  int16_t signedRSSI;

  if (rawRSSI >= 128) {
    signedRSSI =
        static_cast<int16_t>(rawRSSI)
        - 256;
  } else {
    signedRSSI = rawRSSI;
  }

  return (
      static_cast<float>(signedRSSI)
      / 2.0f
  ) - RSSI_OFFSET_DB;
}

bool enterDirectReceiveMode() {
  lastError =
      radio.receiveDirectAsync();

  directModeActive =
      lastError == RADIOLIB_ERR_NONE;

  return directModeActive;
}

}  // namespace

namespace ChickadeeRadio {

bool begin() {
  pinMode(
      Chickadee::CC1101_CS,
      OUTPUT
  );

  digitalWrite(
      Chickadee::CC1101_CS,
      HIGH
  );

  SPI.begin(
      Chickadee::CC1101_SCK,
      Chickadee::CC1101_MISO,
      Chickadee::CC1101_MOSI,
      Chickadee::CC1101_CS
  );

  lastError = radio.begin(
      DEFAULT_FREQUENCY_MHZ,
      4.8,
      5.0,
      203.0,
      10,
      16
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

  if (directModeActive) {
    return enterDirectReceiveMode();
  }

  lastError = radio.startReceive();

  return lastError ==
         RADIOLIB_ERR_NONE;
}

bool startLiveRSSI() {
  if (!radioReady) {
    return false;
  }

  return enterDirectReceiveMode();
}

void stopLiveRSSI() {
  if (!radioReady) {
    return;
  }

  directModeActive = false;

  radio.standby();

  lastError = radio.startReceive();
}

float readRSSI() {
  if (
      !radioReady ||
      !directModeActive
  ) {
    return -120.0f;
  }

  return convertRawRSSI(
      readStatusRegister(
          CC1101_RSSI_REGISTER
      )
  );
}

bool startSpectrumMode() {
  if (!radioReady) {
    return false;
  }

  return enterDirectReceiveMode();
}

bool tuneSpectrum(float frequencyMHz) {
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
   * Re-enter direct receive mode after tuning.
   */
  return enterDirectReceiveMode();
}

float readSpectrumRSSI() {
  if (
      !radioReady ||
      !directModeActive
  ) {
    return -120.0f;
  }

  const uint8_t rawRSSI =
      readStatusRegister(
          CC1101_RSSI_REGISTER
      );

  return convertRawRSSI(rawRSSI);
}

}  // namespace ChickadeeRadio