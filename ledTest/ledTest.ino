#include <FastLED.h>

constexpr int NUM_LEDS = 3;
CRGB leds16[NUM_LEDS];
CRGB leds17[NUM_LEDS];
CRGB leds18[NUM_LEDS];

void setup() {
  FastLED.addLeds<WS2812B, 16, GRB>(leds16, NUM_LEDS);
  FastLED.addLeds<WS2812B, 17, GRB>(leds17, NUM_LEDS);
  FastLED.addLeds<WS2812B, 18, GRB>(leds18, NUM_LEDS);
  FastLED.setBrightness(50);
}

void loop() {
  fill_solid(leds16, NUM_LEDS, CRGB::White);
  fill_solid(leds17, NUM_LEDS, CRGB::White);
  fill_solid(leds18, NUM_LEDS, CRGB::White);
  FastLED.show();
  delay(500);

  FastLED.clear(true);
  delay(500);
}