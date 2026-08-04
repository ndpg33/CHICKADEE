#include "Display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#include "ChickadeeConfig.h"
#include "Menu.h"

namespace {

Adafruit_SSD1306 display(
    Chickadee::OLED_WIDTH,
    Chickadee::OLED_HEIGHT,
    &Wire,
    -1
);

void prepareText(uint8_t textSize = 1) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(textSize);
  display.setTextWrap(false);
  display.setCursor(0, 0);
}

}  // namespace

namespace ChickadeeDisplay {

bool begin() {
  Wire.begin(
      Chickadee::OLED_SDA,
      Chickadee::OLED_SCL
  );

  return display.begin(
      SSD1306_SWITCHCAPVCC,
      Chickadee::OLED_ADDRESS
  );
}

void showBootScreen() {
  prepareText(2);

  display.println("CHICKADEE");

  display.setTextSize(1);
  display.println();
  display.print("Firmware v");
  display.println(Chickadee::VERSION);
  display.println();
  display.println("Initializing...");

  display.display();
}

void showHardwareStatus(
    bool oledReady,
    bool radioReady
) {
  prepareText(1);

  display.println("CHICKADEE STATUS");
  display.println("----------------");

  display.print("OLED:    ");
  display.println(oledReady ? "OK" : "FAILED");

  display.print("CC1101:  ");
  display.println(radioReady ? "OK" : "FAILED");

  display.println();
  display.println("Buttons: READY");

  display.display();
}

void showMainMenu(uint8_t selectedIndex) {
  prepareText(1);

  display.println("CHICKADEE");
  display.drawFastHLine(
      0,
      10,
      Chickadee::OLED_WIDTH,
      SSD1306_WHITE
  );

  for (uint8_t index = 0;
       index < ChickadeeMenu::ITEM_COUNT;
       index++) {

    const int16_t yPosition = 16 + (index * 12);

    display.setCursor(0, yPosition);

    if (index == selectedIndex) {
      display.print("> ");
    } else {
      display.print("  ");
    }

    const auto item =
        static_cast<ChickadeeMenu::Item>(index);

    display.print(
        ChickadeeMenu::getItemLabel(item)
    );
  }

  display.display();
}

void showComingSoon(const char* title) {
  prepareText(1);

  display.println(title);
  display.drawFastHLine(
      0,
      10,
      Chickadee::OLED_WIDTH,
      SSD1306_WHITE
  );

  display.setCursor(0, 24);
  display.println("Coming soon");

  display.setCursor(0, 48);
  display.println("BACK: Main menu");

  display.display();
}

void showLiveRSSI(
    float frequencyMHz,
    float rssi
) {
  prepareText(1);

  // Header
  display.println("LIVE RSSI");

  display.drawFastHLine(
      0,
      10,
      Chickadee::OLED_WIDTH,
      SSD1306_WHITE
  );

  // Frequency
  display.setCursor(0, 14);
  display.print(frequencyMHz, 3);
  display.println(" MHz");

  // RSSI value
  display.setTextSize(2);
  display.setCursor(0, 25);
  display.print(rssi, 1);

  display.setTextSize(1);
  display.print(" dBm");

  // Signal-strength bar
  int barWidth = static_cast<int>(
      map(
          static_cast<long>(rssi),
          -110,
          -30,
          0,
          118
      )
  );

  barWidth = constrain(
      barWidth,
      0,
      118
  );

  display.drawRect(
      3,
      45,
      122,
      9,
      SSD1306_WHITE
  );

  if (barWidth > 0) {
    display.fillRect(
        5,
        47,
        barWidth,
        5,
        SSD1306_WHITE
    );
  }

  // Footer fits within pixels 55–63
  display.setCursor(0, 56);
  display.print("UP/DN:TUNE  BACK:EXIT");

  display.display();
}

void showSpectrum(
    float startMHz,
    float endMHz,
    const float* liveTrace,
    const float* peakTrace,
    uint8_t pointCount,
    float peakFrequency,
    float peakRSSI
) {
  prepareText(1);

  constexpr int16_t GRAPH_LEFT = 18;
  constexpr int16_t GRAPH_RIGHT = 127;

  constexpr int16_t GRAPH_TOP = 12;
  constexpr int16_t GRAPH_BOTTOM = 47;

  constexpr float RSSI_TOP = -30.0f;
  constexpr float RSSI_BOTTOM = -110.0f;

  display.setCursor(0, 0);
  display.print(startMHz, 1);
  display.print("-");
  display.print(endMHz, 1);
  display.print(" MHz");

  // Y-axis labels
  display.setCursor(0, GRAPH_TOP);
  display.print("-30");

  display.setCursor(0, GRAPH_BOTTOM - 6);
  display.print("-110");

  // Axes
  display.drawFastVLine(
      GRAPH_LEFT,
      GRAPH_TOP,
      GRAPH_BOTTOM - GRAPH_TOP + 1,
      SSD1306_WHITE
  );

  display.drawFastHLine(
      GRAPH_LEFT,
      GRAPH_BOTTOM,
      GRAPH_RIGHT - GRAPH_LEFT + 1,
      SSD1306_WHITE
  );

  int16_t previousX = GRAPH_LEFT;
  int16_t previousY = GRAPH_BOTTOM;

  for (
      uint8_t i = 0;
      i < pointCount;
      i++
  ) {
    const int16_t x = map(
        i,
        0,
        pointCount - 1,
        GRAPH_LEFT + 1,
        GRAPH_RIGHT
    );

    float liveValue = constrain(
        liveTrace[i],
        RSSI_BOTTOM,
        RSSI_TOP
    );

    const int16_t liveY = map(
        static_cast<long>(
            liveValue * 10.0f
        ),
        static_cast<long>(
            RSSI_BOTTOM * 10.0f
        ),
        static_cast<long>(
            RSSI_TOP * 10.0f
        ),
        GRAPH_BOTTOM - 1,
        GRAPH_TOP
    );

    if (i > 0) {
      display.drawLine(
          previousX,
          previousY,
          x,
          liveY,
          SSD1306_WHITE
      );
    }

    previousX = x;
    previousY = liveY;

    /*
     * Peak hold is drawn as a dotted trace.
     */
    float peakValue = constrain(
        peakTrace[i],
        RSSI_BOTTOM,
        RSSI_TOP
    );

    const int16_t peakY = map(
        static_cast<long>(
            peakValue * 10.0f
        ),
        static_cast<long>(
            RSSI_BOTTOM * 10.0f
        ),
        static_cast<long>(
            RSSI_TOP * 10.0f
        ),
        GRAPH_BOTTOM - 1,
        GRAPH_TOP
    );

    if ((i % 3) == 0) {
      display.drawPixel(
          x,
          peakY,
          SSD1306_WHITE
      );
    }
  }

  display.setCursor(0, 50);

  display.print("P:");
  display.print(peakFrequency, 2);
  display.print(" ");
  display.print(peakRSSI, 0);
  display.print("dBm");

  display.setCursor(0, 58);
  display.print("UP/DN PAN SEL CLR");

  display.display();
}

}  // namespace ChickadeeDisplay