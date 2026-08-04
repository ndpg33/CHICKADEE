#ifndef CHICKADEE_SPECTRUM_SCANNER_H
#define CHICKADEE_SPECTRUM_SCANNER_H

#include <Arduino.h>

namespace ChickadeeSpectrum {

constexpr uint8_t POINT_COUNT = 104;

bool begin(float startMHz);

void update();

void panUp();
void panDown();

void clearPeaks();

float getStartFrequency();
float getEndFrequency();

float getPeakFrequency();
float getPeakRSSI();

const float* getLiveTrace();
const float* getPeakTrace();

bool hasNewSweep();

}  // namespace ChickadeeSpectrum

#endif