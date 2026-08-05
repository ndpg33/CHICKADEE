#include "SpectrumScanner.h"

#include <Arduino.h>

#include "Radio.h"

namespace {

constexpr float WINDOW_WIDTH_MHZ = 7.0f;
constexpr float PAN_STEP_MHZ = 1.0f;

constexpr float BAND_MIN_MHZ = 387.0f;
constexpr float BAND_MAX_MHZ = 464.0f;

constexpr float MIN_START_MHZ =
    BAND_MIN_MHZ;

constexpr float MAX_START_MHZ =
    BAND_MAX_MHZ -
    WINDOW_WIDTH_MHZ;

// Normal graph scan
constexpr uint32_t WINDOW_SETTLE_US = 2200;
constexpr float WINDOW_BANDWIDTH_KHZ = 203.0f;

// Fast coarse seek
constexpr float COARSE_STEP_MHZ = 0.40f;
constexpr uint32_t COARSE_SETTLE_US = 850;
constexpr float COARSE_BANDWIDTH_KHZ = 650.0f;

// Fine scan around a coarse candidate
constexpr float REFINE_RADIUS_MHZ = 1.0f;
constexpr float REFINE_STEP_MHZ = 0.05f;
constexpr uint32_t REFINE_SETTLE_US = 1600;
constexpr float REFINE_BANDWIDTH_KHZ = 203.0f;

// Nearby fobs should normally exceed this significantly.
constexpr float SEEK_THRESHOLD_DBM = -82.0f;

enum class ScannerMode : uint8_t {
  WindowScan,
  SeekCoarse,
  SeekRefine
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

// Auto Seek state
float seekCurrentFrequencyMHz =
    BAND_MIN_MHZ;

float seekBestFrequencyMHz =
    BAND_MIN_MHZ;

float seekBestRSSI =
    -120.0f;

float coarseCandidateFrequencyMHz =
    BAND_MIN_MHZ;

float refineStartMHz =
    BAND_MIN_MHZ;

float refineEndMHz =
    BAND_MIN_MHZ;

float frequencyForPoint(uint8_t index) {
  const float fraction =
      static_cast<float>(index) /
      static_cast<float>(
          ChickadeeSpectrum::POINT_COUNT -
          1
      );

  return startFrequencyMHz +
         fraction *
         WINDOW_WIDTH_MHZ;
}

/*
 * Use the strongest sample so a brief RF burst
 * is not discarded by a median or average.
 */
float readTransientRSSI() {
  float strongest = -120.0f;

  for (uint8_t sample = 0; sample < 3; sample++) {
    const float reading =
        ChickadeeRadio::
            readSpectrumRSSI();

    if (reading > strongest) {
      strongest = reading;
    }

    delayMicroseconds(80);
  }

  return strongest;
}

void resetLiveTrace() {
  for (
      uint8_t index = 0;
      index <
          ChickadeeSpectrum::POINT_COUNT;
      index++
  ) {
    liveTrace[index] = -120.0f;
  }
}

void resetPeakTrace() {
  for (
      uint8_t index = 0;
      index <
          ChickadeeSpectrum::POINT_COUNT;
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

  ChickadeeRadio::setSpectrumBandwidth(
      WINDOW_BANDWIDTH_KHZ
  );

  restartWindowSweep();
  resetPeakTrace();
}

void placeSeekResultOnGraph(
    float frequencyMHz,
    float rssi
) {
  strongestFrequencyMHz =
      frequencyMHz;

  strongestRSSI =
      rssi;

  const float position =
      (
          frequencyMHz -
          startFrequencyMHz
      ) / WINDOW_WIDTH_MHZ;

  int peakIndex =
      static_cast<int>(
          roundf(
              position *
              (
                  ChickadeeSpectrum::
                      POINT_COUNT - 1
              )
          )
      );

  peakIndex = constrain(
      peakIndex,
      0,
      ChickadeeSpectrum::
          POINT_COUNT - 1
  );

  /*
   * Put the detected measurement into both
   * traces so it appears immediately.
   */
  liveTrace[peakIndex] =
      rssi;

  peakTrace[peakIndex] =
      rssi;
}

void centerWindowOn(float frequencyMHz) {
  setWindowStart(
      frequencyMHz -
      WINDOW_WIDTH_MHZ / 2.0f
  );
}

void beginRefine(float candidateMHz) {
  coarseCandidateFrequencyMHz =
      candidateMHz;

  refineStartMHz = constrain(
      candidateMHz -
          REFINE_RADIUS_MHZ,
      BAND_MIN_MHZ,
      BAND_MAX_MHZ
  );

  refineEndMHz = constrain(
      candidateMHz +
          REFINE_RADIUS_MHZ,
      BAND_MIN_MHZ,
      BAND_MAX_MHZ
  );

  seekCurrentFrequencyMHz =
      refineStartMHz;

  scannerMode =
      ScannerMode::SeekRefine;

  waitingForMeasurement = false;

  ChickadeeRadio::setSpectrumBandwidth(
      REFINE_BANDWIDTH_KHZ
  );
}

void updateWindowScan() {
  if (!waitingForMeasurement) {
    const float frequency =
        frequencyForPoint(
            currentPoint
        );

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
      micros() - tuneStartedAt <
      WINDOW_SETTLE_US
  ) {
    return;
  }

  const float frequency =
      frequencyForPoint(
          currentPoint
      );

  const float rssi =
      readTransientRSSI();

  liveTrace[currentPoint] =
      rssi;

  if (
      rssi >
      peakTrace[currentPoint]
  ) {
    peakTrace[currentPoint] =
        rssi;
  }

  if (rssi > strongestRSSI) {
    strongestRSSI = rssi;

    strongestFrequencyMHz =
        frequency;
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

void updateCoarseSeek() {
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
      micros() - tuneStartedAt <
      COARSE_SETTLE_US
  ) {
    return;
  }

  const float rssi =
      readTransientRSSI();

  if (rssi > seekBestRSSI) {
    seekBestRSSI = rssi;

    seekBestFrequencyMHz =
        seekCurrentFrequencyMHz;
  }

  /*
   * A strong candidate immediately starts
   * a narrow refinement scan.
   */
  if (rssi >= SEEK_THRESHOLD_DBM) {
    beginRefine(
        seekCurrentFrequencyMHz
    );

    return;
  }

  seekCurrentFrequencyMHz +=
      COARSE_STEP_MHZ;

  if (
      seekCurrentFrequencyMHz >
      BAND_MAX_MHZ
  ) {
    seekCurrentFrequencyMHz =
        BAND_MIN_MHZ;
  }

  waitingForMeasurement = false;
}

void updateRefineSeek() {
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
      micros() - tuneStartedAt <
      REFINE_SETTLE_US
  ) {
    return;
  }

  const float rssi =
      readTransientRSSI();

  if (rssi > seekBestRSSI) {
    seekBestRSSI = rssi;

    seekBestFrequencyMHz =
        seekCurrentFrequencyMHz;
  }

  seekCurrentFrequencyMHz +=
      REFINE_STEP_MHZ;

  /*
   * Keep repeating the refined region while
   * Select remains held. This allows repeated
   * key-fob transmissions to improve the result.
   */
  if (
      seekCurrentFrequencyMHz >
      refineEndMHz
  ) {
    seekCurrentFrequencyMHz =
        refineStartMHz;
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

  if (
      !ChickadeeRadio::
          setSpectrumBandwidth(
              WINDOW_BANDWIDTH_KHZ
          )
  ) {
    return false;
  }

  return ChickadeeRadio::
      startSpectrumMode();
}

void update() {
  if (!ChickadeeRadio::isReady()) {
    return;
  }

  switch (scannerMode) {
    case ScannerMode::WindowScan:
      updateWindowScan();
      break;

    case ScannerMode::SeekCoarse:
      updateCoarseSeek();
      break;

    case ScannerMode::SeekRefine:
      updateRefineSeek();
      break;
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
      ScannerMode::SeekCoarse;

  seekCurrentFrequencyMHz =
      BAND_MIN_MHZ;

  seekBestFrequencyMHz =
      BAND_MIN_MHZ;

  seekBestRSSI =
      -120.0f;

  coarseCandidateFrequencyMHz =
      BAND_MIN_MHZ;

  waitingForMeasurement = false;
  newSweepAvailable = false;

  ChickadeeRadio::setSpectrumBandwidth(
      COARSE_BANDWIDTH_KHZ
  );
}

void stopSeekAndCenter() {
  if (!isSeeking()) {
    return;
  }

  const float detectedFrequency =
      seekBestFrequencyMHz;

  const float detectedRSSI =
      seekBestRSSI;

  ChickadeeRadio::
      setSpectrumBandwidth(
          WINDOW_BANDWIDTH_KHZ
      );

  if (
      detectedRSSI >=
      SEEK_THRESHOLD_DBM
  ) {
    /*
     * This resets the normal graph, so preserve
     * the result above before calling it.
     */
    centerWindowOn(
        detectedFrequency
    );

    /*
     * Restore the detected result onto the
     * newly centered graph.
     */
    placeSeekResultOnGraph(
        detectedFrequency,
        detectedRSSI
    );

    newSweepAvailable = true;
  } else {
    scannerMode =
        ScannerMode::WindowScan;

    restartWindowSweep();
  }
}

void cancelSeek() {
  ChickadeeRadio::setSpectrumBandwidth(
      WINDOW_BANDWIDTH_KHZ
  );

  scannerMode =
      ScannerMode::WindowScan;

  restartWindowSweep();
}

bool isSeeking() {
  return
      scannerMode ==
          ScannerMode::SeekCoarse ||
      scannerMode ==
          ScannerMode::SeekRefine;
}

float getSeekFrequency() {
  return seekBestFrequencyMHz;
}

float getSeekRSSI() {
  return seekBestRSSI;
}

float getSeekProgress() {
  if (
      scannerMode ==
      ScannerMode::SeekRefine
  ) {
    const float width =
        refineEndMHz -
        refineStartMHz;

    if (width <= 0.0f) {
      return 1.0f;
    }

    return constrain(
        (
            seekCurrentFrequencyMHz -
            refineStartMHz
        ) / width,
        0.0f,
        1.0f
    );
  }

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