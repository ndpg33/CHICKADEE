#ifndef CHICKADEE_DISPLAY_H
#define CHICKADEE_DISPLAY_H

#include <Arduino.h>

namespace ChickadeeDisplay {

bool begin();

void showBootScreen();

void showHardwareStatus(
    bool oledReady,
    bool radioReady
);

void showMainMenu(uint8_t selectedIndex);

void showComingSoon(
    const char* title
);

void showLiveRSSI(
    float frequencyMHz,
    float rssi
);

}  // namespace ChickadeeDisplay

#endif