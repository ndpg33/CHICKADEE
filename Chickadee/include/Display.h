#ifndef CHICKADEE_DISPLAY_H
#define CHICKADEE_DISPLAY_H

namespace ChickadeeDisplay {

bool begin();

void showBootScreen();
void showHardwareStatus(bool oledReady, bool radioReady);
void showButtonEvent(const char* buttonName, bool pressed);

}  // namespace ChickadeeDisplay

#endif