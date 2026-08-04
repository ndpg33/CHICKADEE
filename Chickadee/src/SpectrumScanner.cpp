#include "SpectrumScanner.h"

#include <Arduino.h>

#include "Radio.h"

namespace {

constexpr float WINDOW_WIDTH_MHZ = 7.0f;
constexpr float PAN_STEP_MHZ = 1.0f;

constexpr float BAND_MIN_MHZ = 387.0f;
constexpr float BAND_MAX_MHZ = 464.0f;

constexpr float MIN_START_MHZ = BAND_MIN_MHZ;
constexpr float MAX_START_MHZ =
    BAND_MAX_MHZ - WINDOW_WIDTH_MHZ;

// Time allowed for the synthesizer and RSSI circuit to settle.
constexpr uint32_t SETTLE_TIME_US = 2200;

float startFrequencyMHz = 430.0f;

float liveTrace[
    ChickadeeSpectrum::POINT_COUNT
];

float peakTrace[
    ChickadeeSpectrum::POINT_COUNT
];

uint8_t currentPoint = 0;

float strongestFrequencyMHz = 430.0f;
float strongestRSSI = -120.0f;

bool waitingForMeasurement = false;
bool newSweepAvailable = false;

uint32_t tuneStartedAt = 0;

float frequencyForPoint(uint8_t index) {
  const float fraction =
      static_cast<float>(index) /
      static_cast<float>(
          ChickadeeSpectrum::POINT_COUNT - 1
      );

  return startFrequencyMHz +
         fraction * WINDOW_WIDTH_MHZ;
}

void resetLiveTrace() {
  for (
      uint8_t i = 0;
      i < ChickadeeSpectrum::POINT_COUNT;
      i++
  ) {
    liveTrace[i] = -120.0f;
  }
}

void resetPeakTrace() {
  for (
      uint8_t i = 0;
      i < ChickadeeSpectrum::POINT_COUNT;
      i++
  ) {
    peakTrace[i] = -120.0f;
  }

  strongestFrequencyMHz =
      startFrequencyMHz;

  strongestRSSI = -120.0f;
}

void restartSweep() {
  currentPoint = 0;
  waitingForMeasurement = false;
  newSweepAvailable = false;
  resetLiveTrace();
}

void setWindowStart(float frequencyMHz) {
  startFrequencyMHz = constrain(
      frequencyMHz,
      MIN_START_MHZ,
      MAX_START_MHZ
  );

  restartSweep();
  resetPeakTrace();
}

}  // namespace

namespace ChickadeeSpectrum {

bool begin(float startMHz) {
  if (!ChickadeeRadio::isReady()) {
    return false;
  }

  startFrequencyMHz = constrain(
      startMHz,
      MIN_START_MHZ,
      MAX_START_MHZ
  );

  resetLiveTrace();
  resetPeakTrace();

  currentPoint = 0;
  waitingForMeasurement = false;
  newSweepAvailable = false;

  return ChickadeeRadio::startSpectrumMode();
}

void update() {
  if (!ChickadeeRadio::isReady()) {
    return;
  }

  /*
   * Stage 1:
   * Tune to the next frequency.
   */
  if (!waitingForMeasurement) {
    const float frequency =
        frequencyForPoint(currentPoint);

    if (
        ChickadeeRadio::tuneSpectrum(
            frequency
        )
    ) {
      tuneStartedAt = micros();
      waitingForMeasurement = true;
    }

    return;
  }

  /*
   * Stage 2:
   * Wait for the receiver and RSSI circuit
   * to settle without blocking button input.
   */
  if (
      micros() - tuneStartedAt
      < SETTLE_TIME_US
  ) {
    return;
  }

  const float frequency =
      frequencyForPoint(currentPoint);

  const float rssi =
      ChickadeeRadio::readSpectrumRSSI();

  liveTrace[currentPoint] = rssi;

  if (rssi > peakTrace[currentPoint]) {
    peakTrace[currentPoint] = rssi;
  }

  if (rssi > strongestRSSI) {
    strongestRSSI = rssi;
    strongestFrequencyMHz = frequency;
  }

  waitingForMeasurement = false;
  currentPoint++;

  /*
   * Finish the sweep and immediately begin
   * another sweep of the same window.
   */
  if (currentPoint >= POINT_COUNT) {
    currentPoint = 0;
    newSweepAvailable = true;
  }
}

void panUp() {
  setWindowStart(
      startFrequencyMHz + PAN_STEP_MHZ
  );
}

void panDown() {
  setWindowStart(
      startFrequencyMHz - PAN_STEP_MHZ
  );
}

void clearPeaks() {
  resetPeakTrace();
}

float getStartFrequency() {
  return startFrequencyMHz;
}

float getEndFrequency() {
  return startFrequencyMHz +
         WINDOW_WIDTH_MHZ;
}

float getPeakFrequency() {
  return strongestFrequencyMHz;
}

float getPeakRSSI() {
  return strongestRSSI;
}

const float* getLiveTrace() {
  return liveTrace;
}

const float* getPeakTrace() {
  return peakTrace;
}

bool hasNewSweep() {
  if (!newSweepAvailable) {
    return false;
  }

  newSweepAvailable = false;
  return true;
}

}  // namespace ChickadeeSpectrum