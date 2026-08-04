#include "Menu.h"

namespace {

uint8_t selectedIndex = 0;

}  // namespace

namespace ChickadeeMenu {

void begin() {
  selectedIndex = 0;
}

void moveUp() {
  if (selectedIndex == 0) {
    selectedIndex = ITEM_COUNT - 1;
  } else {
    selectedIndex--;
  }
}

void moveDown() {
  selectedIndex++;

  if (selectedIndex >= ITEM_COUNT) {
    selectedIndex = 0;
  }
}

uint8_t getSelectedIndex() {
  return selectedIndex;
}

Item getSelectedItem() {
  return static_cast<Item>(selectedIndex);
}

const char* getItemLabel(Item item) {
  switch (item) {
    case Item::SpectrumScan:
      return "Spectrum Scan";

    case Item::LiveRSSI:
      return "Live RSSI";

    case Item::RawCapture:
      return "Raw Capture";

    case Item::Settings:
      return "Settings";

    default:
      return "Unknown";
  }
}

}  // namespace ChickadeeMenu