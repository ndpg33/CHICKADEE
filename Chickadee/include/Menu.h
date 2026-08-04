#ifndef CHICKADEE_MENU_H
#define CHICKADEE_MENU_H

#include <Arduino.h>

namespace ChickadeeMenu {

enum class Item : uint8_t {
  SpectrumScan = 0,
  LiveRSSI,
  RawCapture,
  Settings
};

constexpr uint8_t ITEM_COUNT = 4;

void begin();

void moveUp();
void moveDown();

uint8_t getSelectedIndex();
Item getSelectedItem();

const char* getItemLabel(Item item);

}  // namespace ChickadeeMenu

#endif