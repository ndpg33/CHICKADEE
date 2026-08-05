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

// Normal spectrum scan timing
constexpr uint32_t SETTLE_TIME_US = 2200;

// Auto Seek uses coarser steps for speed
constexpr float SEEK_STEP_MHZ = 0.10f;
constexpr uint32_t SEEK_SETTLE_US = 1700;

// Ignore weak ambient noise as a seek result
constexpr float SEEK_THRESHOLD_DBM = -90.0f;

enum class ScannerMode : uint8_t {
  WindowScan,
  Seek
};

ScannerMode scannerMode =
    ScannerMode::WindowScan;

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

// Seek state
float seekCurrentFrequencyMHz = BAND_MIN_MHZ;
float seekStrongestFrequencyMHz = BAND_MIN_MHZ;
float seekStrongestRSSI = -120.0f;

float frequencyForPoint(uint8_t index) {
  const float fraction =
      static_cast<float>(index) /
      static_cast<float>(
          ChickadeeSpectrum::POINT_COUNT - 1
      );

  return startFrequencyMHz +
         fraction * WINDOW_WIDTH_MHZ;
}

float medianOfThree(
    float first,
    float second,
    float third
) {
  if (first > second) {
    const float temporary = first;
    first = second;
    second = temporary;
  }

  if (second > third) {
    const float temporary = second;
    second = third;
    third = temporary;
  }

  if (first > second) {
    const float temporary = first;
    first = second;
    second = temporary;
  }

  return second;
}

float readFilteredRSSI() {
  const float first =
      ChickadeeRadio::readSpectrumRSSI();

  delayMicroseconds(120);

  const float second =
      ChickadeeRadio::readSpectrumRSSI();

  delayMicroseconds(120);

  const float third =
      ChickadeeRadio::readSpectrumRSSI();

  return medianOfThree(
      first,
      second,
      third
  );
}

void resetLiveTrace() {
  for (
      uint8_t index = 0;
      index < ChickadeeSpectrum::POINT_COUNT;
      index++
  ) {
    liveTrace[index] = -120.0f;
  }
}

void resetPeakTrace() {
  for (
      uint8_t index = 0;
      index < ChickadeeSpectrum::POINT_COUNT;
      index++
  ) {
    peakTrace[index] = -120.0f;
  }

  strongestFrequencyMHz =
      startFrequencyMHz;

  strongestRSSI = -120.0f;
}

void restartWindowSweep() {
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

  scannerMode =
      ScannerMode::WindowScan;

  restartWindowSweep();
  resetPeakTrace();
}

void centerWindowOn(float frequencyMHz) {
  setWindowStart(
      frequencyMHz -
      (WINDOW_WIDTH_MHZ / 2.0f)
  );
}

void updateWindowScan() {
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

  if (
      micros() - tuneStartedAt
      < SETTLE_TIME_US
  ) {
    return;
  }

  const float frequency =
      frequencyForPoint(currentPoint);

  const float rssi =
      readFilteredRSSI();

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

  if (
      currentPoint >=
      ChickadeeSpectrum::POINT_COUNT
  ) {
    currentPoint = 0;
    newSweepAvailable = true;
  }
}

void updateSeek() {
  if (!waitingForMeasurement) {
    if (
        ChickadeeRadio::tuneSpectrum(
            seekCurrentFrequencyMHz
        )
    ) {
      tuneStartedAt = micros();
      waitingForMeasurement = true;
    }

    return;
  }

  if (
      micros() - tuneStartedAt
      < SEEK_SETTLE_US
  ) {
    return;
  }

  const float rssi =
      readFilteredRSSI();

  if (
      rssi >= SEEK_THRESHOLD_DBM &&
      rssi > seekStrongestRSSI
  ) {
    seekStrongestRSSI = rssi;

    seekStrongestFrequencyMHz =
        seekCurrentFrequencyMHz;
  }

  seekCurrentFrequencyMHz +=
      SEEK_STEP_MHZ;

  if (
      seekCurrentFrequencyMHz >
      BAND_MAX_MHZ
  ) {
    seekCurrentFrequencyMHz =
        BAND_MIN_MHZ;
  }

  waitingForMeasurement = false;
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

  scannerMode =
      ScannerMode::WindowScan;

  currentPoint = 0;
  waitingForMeasurement = false;
  newSweepAvailable = false;

  return ChickadeeRadio::
      startSpectrumMode();
}

void update() {
  if (!ChickadeeRadio::isReady()) {
    return;
  }

  if (
      scannerMode ==
      ScannerMode::Seek
  ) {
    updateSeek();
  } else {
    updateWindowScan();
  }
}

void panUp() {
  if (isSeeking()) {
    return;
  }

  setWindowStart(
      startFrequencyMHz +
      PAN_STEP_MHZ
  );
}

void panDown() {
  if (isSeeking()) {
    return;
  }

  setWindowStart(
      startFrequencyMHz -
      PAN_STEP_MHZ
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

void startSeek() {
  scannerMode =
      ScannerMode::Seek;

  seekCurrentFrequencyMHz =
      BAND_MIN_MHZ;

  seekStrongestFrequencyMHz =
      BAND_MIN_MHZ;

  seekStrongestRSSI =
      -120.0f;

  waitingForMeasurement = false;
  newSweepAvailable = false;
}

void stopSeekAndCenter() {
  if (
      scannerMode !=
      ScannerMode::Seek
  ) {
    return;
  }

  if (
      seekStrongestRSSI >=
      SEEK_THRESHOLD_DBM
  ) {
    centerWindowOn(
        seekStrongestFrequencyMHz
    );
  } else {
    scannerMode =
        ScannerMode::WindowScan;

    restartWindowSweep();
  }
}

void cancelSeek() {
  scannerMode =
      ScannerMode::WindowScan;

  restartWindowSweep();
}

bool isSeeking() {
  return scannerMode ==
         ScannerMode::Seek;
}

float getSeekFrequency() {
  return seekStrongestFrequencyMHz;
}

float getSeekRSSI() {
  return seekStrongestRSSI;
}

float getSeekProgress() {
  return constrain(
      (
          seekCurrentFrequencyMHz -
          BAND_MIN_MHZ
      ) /
      (
          BAND_MAX_MHZ -
          BAND_MIN_MHZ
      ),
      0.0f,
      1.0f
  );
}

}  // namespace ChickadeeSpectrum