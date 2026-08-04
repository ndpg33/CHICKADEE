# Chickadee

Chickadee is a portable ESP32-based sub-GHz RF analysis tool built around the
CC1101 transceiver.

The project is named after the black-capped chickadee, the state bird of
Massachusetts.

## Current Features

- ESP32 control
- CC1101 sub-GHz receiver
- OLED user interface
- Four-button navigation
- RSSI monitoring
- Frequency scanning
- Packet activity detection
- Serial-monitor diagnostics

## Hardware

- ESP32 development board
- CC1101 RF transceiver
- 0.96-inch I2C OLED display
- Four momentary push buttons
- Perfboard
- Antenna suitable for the selected frequency range

## Pin Configuration

### CC1101

| CC1101 | ESP32 |
|---|---:|
| VCC | 3.3V |
| GND | GND |
| SCK | GPIO 18 |
| MISO | GPIO 19 |
| MOSI | GPIO 23 |
| CSN | GPIO 5 |
| GDO0 | GPIO 4 |
| GDO2 | GPIO 27 |

### OLED

| OLED | ESP32 |
|---|---:|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### Buttons

| Function | ESP32 |
|---|---:|
| Up | GPIO 32 |
| Down | GPIO 33 |
| Select | GPIO 25 |
| Back | GPIO 26 |


## Repository Structure

- `firmware/` – Main ESP32 firmware
- `hardware/` – Wiring information, diagrams, and build photos
- `docs/` – Project documentation
- `examples/` – Standalone CC1101 and display tests

## Responsible Use

Chickadee is intended for receiving and analyzing signals from equipment you
own or have permission to test.

It should not be used to interfere with radio communications, bypass access
controls, or capture private communications.

## License

This project is licensed under the MIT License.