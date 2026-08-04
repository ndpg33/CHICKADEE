#ifndef CHICKADEE_RADIO_H
#define CHICKADEE_RADIO_H

namespace ChickadeeRadio {

bool begin();

bool isReady();

int getLastError();

float getFrequency();

}  // namespace ChickadeeRadio

#endif