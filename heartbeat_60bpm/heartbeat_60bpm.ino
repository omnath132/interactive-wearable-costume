/*
 * WS2812 "bum-bum" heartbeat at a fixed rate - ESP32
 *
 *   WS2812: DATA = GPIO 32, plus 5V and GND to the strip
 *
 * No sensor. The strip idles at a faint red and beats at BPM, with the
 * same lub-dub shape as the MAX30102 sketch: a full-brightness "bum",
 * then a smaller "bum" after a gap proportional to the beat interval.
 *
 * Library: Adafruit NeoPixel (Library Manager).
 *
 * This doubles as a control test. It drives the strip exactly the way the
 * heartbeat sketch does, but with NO I2C and NO serial streaming. If the
 * strip flashes green here too, the sensor code is not involved and the
 * problem is the strip or the wiring. If it stays clean here but not
 * there, the surrounding traffic is implicated after all.
 */

#include <Adafruit_NeoPixel.h>

// ---- strip ----
#define STRIP_PIN   23      // WS2812 data (NOTE: 34/35/36/39 are input-only!)
#define STRIP_LEDS  18
#define COLOR_ORDER NEO_GRB
#define BRIGHTNESS  120     // master brightness, 0..255

// ---- rate ----
#define BPM          60     // beats per minute

// ---- look ----
#define GLOW_FLOOR   14     // always-on faint red baseline (0..255)
#define BUM_LEVEL   255     // brightness of the main beat
#define DUB_LEVEL   110     // brightness of the second (echo) beat
#define DUB_FRACTION 0.20   // gap = this fraction of the beat interval
#define FADE_RATE   0.78    // per-frame decay of a pulse (~120 ms)

// ---- frame pacing ----
#define RENDER_MS    33     // ~30 fps - one show() per frame, no more
#define REFRESH_MS  500     // re-send even if unchanged, to heal glitches
#define GLOW_STEP     6     // brightness quantisation, fewer distinct frames

Adafruit_NeoPixel strip(STRIP_LEDS, STRIP_PIN, COLOR_ORDER + NEO_KHZ800);

const unsigned long BEAT_MS = 60000UL / BPM;              // ms between beats
const unsigned long DUB_MS  = (unsigned long)(BEAT_MS * DUB_FRACTION);

// ---- animation state ----
float glow = 0;                  // 0..255, current pulse level (pre-floor)
unsigned long lastBeat = 0;
unsigned long dubAt = 0;
bool dubPending = false;

// =====================================================================
// Renderer - the only code here that touches the strip.
//
// Every pixel is rewritten every frame, frames are paced, and the frame
// is re-sent periodically so a corrupted one gets overwritten rather
// than sticking around.
// =====================================================================
void renderStrip() {
  static unsigned long lastFrame = 0;
  static unsigned long lastSent = 0;
  static int lastValue = -1;

  unsigned long now = millis();
  if (now - lastFrame < RENDER_MS) return;
  lastFrame = now;

  if (dubPending && now >= dubAt) {        // fire the queued second thump
    dubPending = false;
    if (glow < DUB_LEVEL) glow = DUB_LEVEL;
  }
  glow *= FADE_RATE;
  if (glow < 1) glow = 0;

  // Never go fully dark: the beats ride on top of a faint red floor.
  int v = (int)(GLOW_FLOOR + glow * (255.0 - GLOW_FLOOR) / 255.0);
  if (v < 0)   v = 0;
  if (v > 255) v = 255;
  if (v < 255) v = (v / GLOW_STEP) * GLOW_STEP;   // quantise, fewer frames

  if (v == lastValue && now - lastSent < REFRESH_MS) return;
  lastValue = v;
  lastSent = now;

  uint32_t c = strip.Color(v, 0, 0);       // red only
  for (int i = 0; i < STRIP_LEDS; i++) strip.setPixelColor(i, c);
  strip.show();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== WS2812 heartbeat @ %d bpm ===\n", BPM);
  Serial.printf("beat every %lu ms, echo %lu ms after\n", BEAT_MS, DUB_MS);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();

  lastBeat = millis();
}

void loop() {
  unsigned long now = millis();

  // fire a beat on schedule
  if (now - lastBeat >= BEAT_MS) {
    lastBeat += BEAT_MS;                   // fixed cadence, no drift
    glow = BUM_LEVEL;
    dubAt = now + DUB_MS;
    dubPending = true;
  }

  renderStrip();
}
