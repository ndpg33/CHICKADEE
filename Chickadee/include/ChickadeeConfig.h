#ifndef CHICKADEE_CONFIG_H
#define CHICKADEE_CONFIG_H

#include <Arduino.h>

namespace Chickadee {

// Firmware
constexpr char VERSION[] = "0.1.0";

// OLED
constexpr uint8_t OLED_SDA = 21;
constexpr uint8_t OLED_SCL = 22;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;

// CC1101
constexpr uint8_t CC1101_CS = 5;
constexpr uint8_t CC1101_GDO0 = 4;
constexpr uint8_t CC1101_GDO2 = 2;
constexpr uint8_t CC1101_SCK = 18;
constexpr uint8_t CC1101_MISO = 19;
constexpr uint8_t CC1101_MOSI = 23;

// Buttons
constexpr uint8_t BTN_UP = 33;
constexpr uint8_t BTN_DOWN = 32;
constexpr uint8_t BTN_SELECT = 25;
constexpr uint8_t BTN_BACK = 26;

}  // namespace Chickadee

#endif