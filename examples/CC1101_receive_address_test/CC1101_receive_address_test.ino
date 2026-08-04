#include <RadioLib.h>
#include <SPI.h>

// ESP32 connections
constexpr uint8_t CC1101_CS   = 5;
constexpr uint8_t CC1101_GDO0 = 4;
constexpr uint8_t CC1101_GDO2 = 2;

constexpr uint8_t SPI_SCK  = 18;
constexpr uint8_t SPI_MISO = 19;
constexpr uint8_t SPI_MOSI = 23;

// CC1101 status-register address
constexpr uint8_t CC1101_RSSI_REG = 0x34;

// For reading CC1101 status registers:
// bit 7 = read
// bit 6 = burst/status-register access
constexpr uint8_t CC1101_STATUS_READ = 0xC0;

CC1101 radio = new Module(
  CC1101_CS,
  CC1101_GDO0,
  RADIOLIB_NC,
  CC1101_GDO2
);

// Scanner settings
constexpr float START_MHZ = 430.0;
constexpr float END_MHZ   = 437.0;
constexpr float STEP_MHZ  = 0.05;    // 50 kHz

constexpr uint8_t SAMPLES_PER_FREQUENCY = 5;
constexpr uint16_t RX_SETTLE_US = 2500;

constexpr float PRINT_THRESHOLD_DBM = -85.0;

// Typical CC1101 RSSI offset.
// This can vary slightly depending on receiver configuration.
constexpr float RSSI_OFFSET_DB = 74.0;

SPISettings cc1101SPISettings(
  4000000,        // 4 MHz SPI clock
  MSBFIRST,
  SPI_MODE0
);

uint8_t readCC1101StatusRegister(uint8_t address) {
  SPI.beginTransaction(cc1101SPISettings);

  digitalWrite(CC1101_CS, LOW);

  // Wait until the CC1101 indicates that its oscillator is ready.
  uint32_t timeoutStart = micros();

  while (digitalRead(SPI_MISO) == HIGH) {
    if (micros() - timeoutStart > 1000) {
      break;
    }
  }

  SPI.transfer(address | CC1101_STATUS_READ);
  uint8_t value = SPI.transfer(0x00);

  digitalWrite(CC1101_CS, HIGH);
  SPI.endTransaction();

  return value;
}

float readCurrentRSSI() {
  uint8_t rawRSSI = readCC1101StatusRegister(CC1101_RSSI_REG);

  int16_t signedRSSI;

  if (rawRSSI >= 128) {
    signedRSSI = static_cast<int16_t>(rawRSSI) - 256;
  } else {
    signedRSSI = rawRSSI;
  }

  return (signedRSSI / 2.0f) - RSSI_OFFSET_DB;
}

float measureFrequencyRSSI(float frequencyMHz) {
  int state = radio.setFrequency(frequencyMHz);

  if (state != RADIOLIB_ERR_NONE) {
    return -200.0;
  }

  /*
   * Re-enter direct asynchronous reception after tuning.
   * In this mode the CC1101 continuously updates its RSSI measurement.
   */
  state = radio.receiveDirectAsync();

  if (state != RADIOLIB_ERR_NONE) {
    return -200.0;
  }

  delayMicroseconds(RX_SETTLE_US);

  // Keep the strongest result from several quick samples.
  float strongestRSSI = -200.0;

  for (uint8_t i = 0; i < SAMPLES_PER_FREQUENCY; i++) {
    float rssi = readCurrentRSSI();

    if (rssi > strongestRSSI) {
      strongestRSSI = rssi;
    }

    delayMicroseconds(500);
  }

  return strongestRSSI;
}

void runScan() {
  float peakFrequency = START_MHZ;
  float peakRSSI = -200.0;

  Serial.println();
  Serial.println("========================================");
  Serial.print("Scanning ");
  Serial.print(START_MHZ, 3);
  Serial.print(" to ");
  Serial.print(END_MHZ, 3);
  Serial.println(" MHz");
  Serial.println("========================================");

  for (float frequency = START_MHZ;
       frequency <= END_MHZ + 0.001f;
       frequency += STEP_MHZ) {

    float rssi = measureFrequencyRSSI(frequency);

    if (rssi <= -190.0) {
      Serial.print("Radio error at ");
      Serial.print(frequency, 3);
      Serial.println(" MHz");
      continue;
    }

    if (rssi > peakRSSI) {
      peakRSSI = rssi;
      peakFrequency = frequency;
    }

    if (rssi >= PRINT_THRESHOLD_DBM) {
      Serial.print(frequency, 3);
      Serial.print(" MHz   ");
      Serial.print(rssi, 1);
      Serial.print(" dBm   ");

      // Simple text signal-strength bar
      int barLength = static_cast<int>((rssi + 100.0f) / 2.0f);
      barLength = constrain(barLength, 0, 35);

      for (int i = 0; i < barLength; i++) {
        Serial.print('#');
      }

      Serial.println();
    }
  }

  Serial.println("----------------------------------------");
  Serial.print("Strongest reading: ");
  Serial.print(peakFrequency, 3);
  Serial.print(" MHz at ");
  Serial.print(peakRSSI, 1);
  Serial.println(" dBm");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(CC1101_CS, OUTPUT);
  digitalWrite(CC1101_CS, HIGH);

  SPI.begin(
    SPI_SCK,
    SPI_MISO,
    SPI_MOSI,
    CC1101_CS
  );

  Serial.println();
  Serial.println("CC1101 direct-register RSSI scanner");

  int state = radio.begin(
    433.92,  // Initial frequency
    4.8,     // Bit rate
    5.0,     // Frequency deviation
    203.0,   // Receiver bandwidth
    10,
    16
  );

  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("CC1101 initialization failed: ");
    Serial.println(state);

    while (true) {
      delay(1000);
    }
  }

  state = radio.receiveDirectAsync();

  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Could not enter direct receive mode: ");
    Serial.println(state);

    while (true) {
      delay(1000);
    }
  }

  Serial.println("CC1101 initialized successfully.");
}

void loop() {
  runScan();
  delay(500);
}