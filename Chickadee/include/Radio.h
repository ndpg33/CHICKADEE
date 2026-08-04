#ifndef CHICKADEE_RADIO_H
#define CHICKADEE_RADIO_H

namespace ChickadeeRadio {

bool begin();
bool isReady();

int getLastError();

float getFrequency();
bool setFrequency(float frequencyMHz);

bool startLiveRSSI();
void stopLiveRSSI();

float readRSSI();

}  // namespace ChickadeeRadio

#endif