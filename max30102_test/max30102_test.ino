/*
 * MAX30102 heart-rate sensor -> WS2812 strip "bum-bum" - ESP32
 *
 *   MAX30102:  SDA = GPIO 21, SCL = GPIO 22, VIN -> 3V3, GND -> GND
 *   WS2812 :   DATA = GPIO 32 (+ 5V and GND to the strip)
 *
 * Libraries: Adafruit NeoPixel (Library Manager). Sensor is driven
 * directly over Wire - no sensor library needed.
 * Serial Monitor / Plotter @ 115200.
 *
 * What it does, in order:
 *   1) Scans the I2C bus and lists every device found
 *   2) Checks for the MAX30102 at 0x57 and verifies its part ID (0x15)
 *   3) Reads the die temperature (proves register R/W works)
 *   4) Detects finger on/off
 *   5) Detects heartbeats from the SHARP DROP in the AC signal each beat
 *   6) Drives the strip: faint red baseline, full-brightness "bum" on
 *      each beat, smaller "bum" after a gap that scales with your rate
 *
 * ---------------------------------------------------------------------
 * LED DRIVING RULES (this is what stops the random green flashes)
 *
 * A WS2812 frame is a chain of 0.4/0.8us pulses with no clock line - the
 * timing IS the data. If anything preempts the ESP32 mid-frame, a bit
 * gets stretched and latched as the wrong value. NEO_GRB sends the green
 * byte FIRST, so a corrupted frame usually shows up as a green flash.
 *
 * So this sketch obeys three rules:
 *
 *   1. ONE render point. Nothing anywhere else calls strip.show(). The
 *      beat detector only sets numbers; renderStrip() is the sole owner
 *      of the strip. No show() inside the I2C drain loop, ever.
 *   2. UART idle before show(). Serial.flush() waits for the transmit
 *      buffer to empty, so the UART interrupt is not firing while the
 *      frame goes out.
 *   3. Full frame, every frame, plus a periodic re-send. Every pixel is
 *      rewritten each time and the frame is re-sent at least every
 *      REFRESH_MS, so a corrupted frame is overwritten instead of
 *      sticking around.
 *
 * Graphs: open Tools > Serial Plotter to see:
 *        AC     - your pulse waveform
 *        Slope  - per-sample change of AC (beats = deep negative dip)
 *        Trig   - the (negative) slope level that counts as a beat
 *        Beat   - spikes up for a moment on every detected beat
 *        Motion - dips DOWN while a movement artifact is being rejected
 *        Glow   - what the strip is actually doing
 *        BPM    - smoothed heart rate (flat line at your bpm)
 *
 * Set PLOT to 0 to silence the plotter. If green flashes ever come back,
 * that is the first thing to try - it removes the UART from the picture
 * entirely and tells you whether serial contention is the cause.
 */

#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define PLOT        1       // 1 = stream plotter data, 0 = silent

#define SDA_PIN     21
#define SCL_PIN     22

// ---- LED strip ----
#define STRIP_PIN   23     // WS2812 data (NOTE: 34/35/36/39 are input-only!)
#define STRIP_LEDS  18      // how many LEDs to light
#define COLOR_ORDER NEO_GRB
#define BRIGHTNESS  120     // master brightness, 0..255

#define RENDER_MS   33      // ~30 fps - one show() per frame, no more
#define REFRESH_MS  500     // re-send even if unchanged, to heal glitches
#define GLOW_STEP    6      // brightness quantisation - see renderStrip()
#define POLL_MS      5      // sensor FIFO poll interval (200 Hz vs 25 sps)

// ---- heartbeat look ----
#define GLOW_FLOOR   14     // always-on faint red baseline (0..255)
#define BUM_LEVEL   255     // brightness of the main beat
#define DUB_LEVEL   110     // brightness of the second (echo) beat
#define DUB_FRACTION 0.20   // gap = this fraction of the beat interval
#define DUB_MIN      90     // ms, clamp for very fast rates
#define DUB_MAX     240     // ms, clamp for very slow rates
#define DUB_DELAY   170     // ms, fallback before a rhythm is known
#define FADE_RATE   0.78    // per-frame decay of a pulse (~120 ms)

Adafruit_NeoPixel strip(STRIP_LEDS, STRIP_PIN, COLOR_ORDER + NEO_KHZ800);

// ---- MAX30102 registers ----
#define MAX_ADDR        0x57
#define REG_INT_STAT1   0x00
#define REG_FIFO_WR     0x04
#define REG_FIFO_OVF    0x05
#define REG_FIFO_RD     0x06
#define REG_FIFO_DATA   0x07
#define REG_FIFO_CFG    0x08
#define REG_MODE_CFG    0x09
#define REG_SPO2_CFG    0x0A
#define REG_LED1_PA     0x0C   // Red LED current
#define REG_LED2_PA     0x0D   // IR LED current
#define REG_TEMP_INT    0x1F
#define REG_TEMP_FRAC   0x20
#define REG_TEMP_EN     0x21
#define REG_REV_ID      0xFE
#define REG_PART_ID     0xFF   // reads 0x15 on MAX30102

// =====================================================================
// I2C helpers
// =====================================================================
bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MAX_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(MAX_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;   // repeated start
  if (Wire.requestFrom((uint8_t)MAX_ADDR, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

uint8_t readReg(uint8_t reg) {
  uint8_t v = 0;
  readRegs(reg, &v, 1);
  return v;
}

// =====================================================================
// Sensor bring-up
// =====================================================================
int scanBus() {
  Serial.println(F("\n[1] Scanning I2C bus (SDA=21, SCL=22)..."));
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("    found device at 0x%02X%s\n", a,
                    a == MAX_ADDR ? "  <-- MAX3010x address" : "");
      found++;
    }
  }
  if (!found) Serial.println(F("    NOTHING FOUND - check VIN/GND/SDA/SCL wiring"));
  else        Serial.printf("    %d device(s) total\n", found);
  return found;
}

bool checkPartId() {
  Serial.println(F("\n[2] Checking part ID at 0x57..."));
  uint8_t part = readReg(REG_PART_ID);
  uint8_t rev  = readReg(REG_REV_ID);
  Serial.printf("    part ID = 0x%02X, revision = 0x%02X\n", part, rev);
  if (part == 0x15) { Serial.println(F("    OK: MAX30102/30105 confirmed")); return true; }
  if (part == 0x11) { Serial.println(F("    This is a MAX30100 (older chip, different registers)")); return false; }
  Serial.println(F("    Unexpected ID - wrong chip or bad communication"));
  return false;
}

void configureSensor() {
  writeReg(REG_MODE_CFG, 0x40);            // reset
  delay(100);
  readReg(REG_INT_STAT1);                  // clear power-ready flag
  // FIFO: sample averaging x8 (hardware noise smoothing), rollover on
  writeReg(REG_FIFO_CFG, 0x70);
  // clear FIFO pointers
  writeReg(REG_FIFO_WR, 0); writeReg(REG_FIFO_OVF, 0); writeReg(REG_FIFO_RD, 0);
  // mode: SpO2 (Red + IR active)
  writeReg(REG_MODE_CFG, 0x03);
  // SpO2 config: ADC range 4096nA, 200 samples/s, 411us pulse (18-bit)
  // (200 sps / x8 averaging = 25 clean samples/s reaching the sketch)
  writeReg(REG_SPO2_CFG, 0x2B);
  // LED currents: ~12 mA each (raise toward 0x7F/25mA if signal is weak)
  writeReg(REG_LED1_PA, 0x3C);
  writeReg(REG_LED2_PA, 0x3C);
}

bool readTemperature() {
  Serial.println(F("\n[3] Reading die temperature..."));
  writeReg(REG_TEMP_EN, 0x01);
  delay(50);
  int8_t  ti = (int8_t)readReg(REG_TEMP_INT);
  uint8_t tf = readReg(REG_TEMP_FRAC);
  float t = ti + tf * 0.0625;
  Serial.printf("    die temp = %.2f C\n", t);
  bool ok = (t > 0 && t < 60);
  Serial.println(ok ? F("    OK: plausible value, register access works")
                    : F("    value looks wrong - communication problem?"));
  return ok;
}

// read one Red+IR sample pair from the FIFO; returns false if none waiting
bool readSample(uint32_t& red, uint32_t& ir) {
  uint8_t wr = readReg(REG_FIFO_WR), rd = readReg(REG_FIFO_RD);
  if (wr == rd) return false;                 // FIFO empty
  uint8_t d[6];
  if (!readRegs(REG_FIFO_DATA, d, 6)) return false;
  red = ((uint32_t)d[0] << 16 | (uint32_t)d[1] << 8 | d[2]) & 0x3FFFF;  // 18-bit
  ir  = ((uint32_t)d[3] << 16 | (uint32_t)d[4] << 8 | d[5]) & 0x3FFFF;
  return true;
}

// =====================================================================
// Beat detector - sharp-drop (falling edge) method
//
// Each heartbeat produces one steep drop in the AC signal. We watch the
// per-sample slope; a dip below an adaptive negative trigger = one beat.
// This block owns NO hardware. It only produces numbers.
// =====================================================================
float dcIr = 0;                 // slow-moving DC baseline
float acSmooth = 0;             // lightly smoothed AC signal
float acPrev = 0;               // previous AC sample (for slope)
float slope = 0;                // per-sample change of AC
float envelope = 0;             // recent peak amplitude of the AC signal
float slopeTrig = -4;           // adaptive negative slope trigger (plotted)
float bpmAvg = 0;
float avgInterval = 0;          // running beat-to-beat interval, ms
unsigned long lastBeat = 0;
bool  dropArmed = true;         // re-arms after the signal starts rising
int   beatMarkerSamples = 0;    // >0 -> Beat trace is spiking
float lastIrRaw = 0;            // for motion (jump) detection
float avgJump = 0;              // typical sample-to-sample |change| of raw IR
unsigned long motionHoldUntil = 0;

// ---- what the renderer reads ----
float glow = 0;                 // 0..255, current pulse level (pre-floor)
unsigned long dubAt = 0;        // when to fire the second thump
bool  dubPending = false;

void resetDetector() {
  dcIr = 0; acSmooth = 0; acPrev = 0; slope = 0; envelope = 0; slopeTrig = -4;
  bpmAvg = 0; avgInterval = 0; lastBeat = 0; dropArmed = true;
  beatMarkerSamples = 0; lastIrRaw = 0; avgJump = 0; motionHoldUntil = 0;
  glow = 0; dubPending = false; dubAt = 0;
}

// Fires the "bum", and queues the "bum". The gap scales with the measured
// beat interval so it stays proportional at any rate instead of crowding
// the next beat when the pulse is fast.
void onBeat() {
  beatMarkerSamples = 5;               // spike the Beat trace
  glow = BUM_LEVEL;

  float gap = (avgInterval > 0) ? avgInterval * DUB_FRACTION : DUB_DELAY;
  if (gap < DUB_MIN) gap = DUB_MIN;
  if (gap > DUB_MAX) gap = DUB_MAX;
  dubAt = millis() + (unsigned long)gap;
  dubPending = true;
}

void feedBeatDetector(uint32_t ir) {
  unsigned long now = millis();

  // 0) MOTION GUARD: a heartbeat changes the signal gradually; moving the
  //    finger makes it jump. The threshold adapts to how jumpy the signal
  //    normally is (avgJump), NOT to `envelope` - keying it off envelope
  //    deadlocks, because envelope starts at 0, every beat then looks like
  //    motion, and envelope can never grow.
  if (lastIrRaw != 0) {
    float jump = fabs((float)ir - lastIrRaw);
    if (avgJump == 0) avgJump = jump;
    if (jump > avgJump * 6 + 400) motionHoldUntil = now + 400;
    else avgJump += (jump - avgJump) * 0.05;   // only learn from calm samples
  }
  lastIrRaw = ir;
  bool motion = now < motionHoldUntil;

  // 1) split signal into DC baseline + AC (pulse) part, compute slope
  if (dcIr == 0) dcIr = ir;
  dcIr += (ir - dcIr) * (motion ? 0.30 : 0.05);   // re-settle fast on motion
  float ac = ir - dcIr;
  acSmooth += (ac - acSmooth) * 0.5;      // light noise smoothing
  slope = acSmooth - acPrev;              // change per sample (25 sps)
  acPrev = acSmooth;

  // 2) track how big recent pulses are. Done BEFORE the motion bail-out so
  //    the envelope always has a real value, even if the guard is firing
  //    too eagerly.
  envelope *= motion ? 0.99 : 0.995;
  if (acSmooth > envelope) envelope = acSmooth;

  if (motion) {                            // don't trigger from junk
    dropArmed = false;                     // re-arm only on a clean rise
    return;
  }

  // 3) adaptive slope trigger: the beat's drop is steep, ~proportional to
  //    the pulse amplitude. Trigger at 20% of envelope per sample, with a
  //    floor of 4 counts so noise can't fire it.
  slopeTrig = -(envelope * 0.20);
  if (slopeTrig > -4) slopeTrig = -4;

  // 4) a sharp drop while armed = one beat
  if (dropArmed && slope < slopeTrig) {
    dropArmed = false;                     // one trigger per drop
    // refractory period: ignore re-triggers within 280 ms (>214 bpm)
    if (!lastBeat || now - lastBeat > 280) {
      if (lastBeat) {
        unsigned long interval = now - lastBeat;
        float b = 60000.0 / interval;
        // plausibility: within the sanity window AND not wildly different
        // from the recent rhythm (rejects leftover artifacts)
        bool plausible = (b > 35 && b < 190) &&
                         (avgInterval == 0 ||
                          (interval > avgInterval * 0.55 && interval < avgInterval * 1.8));
        if (plausible) {
          avgInterval = (avgInterval == 0) ? interval : avgInterval * 0.7 + interval * 0.3;
          bpmAvg = (bpmAvg == 0) ? b : bpmAvg * 0.75 + b * 0.25;
          onBeat();                        // only plausible beats flash
        }
      } else {
        onBeat();                          // very first beat after settling
      }
      lastBeat = now;
    }
  } else if (!dropArmed && slope > 0.5) {
    dropArmed = true;                      // signal rising again -> re-arm
  }
}

// =====================================================================
// Renderer - THE ONLY CODE IN THIS SKETCH THAT TOUCHES THE STRIP
//
// Called once per pass through loop(), and only from there. It advances
// the pulse envelope, composes a complete frame, and pushes it out.
// =====================================================================
void renderStrip() {
  static unsigned long lastFrame = 0;
  static unsigned long lastSent = 0;
  static int lastValue = -1;

  unsigned long now = millis();
  if (now - lastFrame < RENDER_MS) return;
  lastFrame = now;

  // --- advance the animation ---
  if (dubPending && now >= dubAt) {        // fire the queued second thump
    dubPending = false;
    if (glow < DUB_LEVEL) glow = DUB_LEVEL;
  }
  glow *= FADE_RATE;
  if (glow < 1) glow = 0;

  // Never go fully dark: the strip idles at a faint red and the beats ride
  // on top of it. Headroom is scaled so the floor does not just clip the
  // bottom off the pulse.
  int v = (int)(GLOW_FLOOR + glow * (255.0 - GLOW_FLOOR) / 255.0);
  if (v < 0)   v = 0;
  if (v > 255) v = 255;

  // Quantise the brightness to GLOW_STEP levels. Every distinct value is
  // one more frame on the wire, and each frame is a chance for a marginal
  // strip to latch a bad bit. Snapping to steps means a slow fade emits a
  // handful of frames instead of one per tick - invisible to the eye at
  // this step size, but far less traffic. Full brightness still lands on
  // 255 exactly, so the beat does not lose its punch.
  if (v < 255) v = (v / GLOW_STEP) * GLOW_STEP;

  // --- decide whether this frame needs to go out ---
  // Re-send even when nothing changed, so a corrupted frame gets
  // overwritten instead of sticking until the next beat.
  if (v == lastValue && now - lastSent < REFRESH_MS) return;
  lastValue = v;
  lastSent = now;

  // --- compose the full frame, then push it ---
  // Every pixel is rewritten every time; no partial updates, so there is
  // no stale state that a dropped frame could leave behind.
  uint32_t c = strip.Color(v, 0, 0);       // red only
  for (int i = 0; i < STRIP_LEDS; i++) strip.setPixelColor(i, c);

  // Wait for the UART to go idle first. Serial.flush() blocks until the
  // transmit buffer has drained, so its interrupt is not firing partway
  // through the frame and stretching a bit.
  Serial.flush();
  strip.show();
}

// =====================================================================
// Setup / loop
// =====================================================================
bool fingerOn = false;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("\n=== MAX30102 -> WS2812 heartbeat ==="));

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();                            // all off

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  if (scanBus() == 0)  { Serial.println(F("\nFix wiring, then press reset.")); while (true) delay(1000); }
  if (!checkPartId())  { Serial.println(F("\nStopping here.")); while (true) delay(1000); }

  configureSensor();
  readTemperature();

  Serial.println(F("\n[4] Ready - open Tools > Serial Plotter, then rest a finger"));
  Serial.println(F("    GENTLY on the sensor window.\n"));
}

void loop() {
  // 1) Drain every sample waiting in the sensor FIFO. Note what is NOT
  //    here: no strip.show(). Rendering never happens inside this loop,
  //    so a frame can never be interleaved with I2C traffic.
  //
  //    Polling is paced to POLL_MS. Left unthrottled, readSample() runs
  //    back-to-back and each empty check is still two register reads, so
  //    the I2C peripheral is busy essentially 100% of the time and there
  //    is never a quiet moment for a frame to go out. The sensor only
  //    produces 25 samples/s, so 200 Hz polling is already 8x oversampled.
  static unsigned long lastPoll = 0;
  if (millis() - lastPoll >= POLL_MS) {
    lastPoll = millis();

    uint32_t red, ir;
    while (readSample(red, ir)) {
      // finger detection: IR jumps way up when skin covers the window
      bool f = ir > 20000;
      if (f != fingerOn) { fingerOn = f; resetDetector(); }

      if (fingerOn) feedBeatDetector(ir);

#if PLOT
      float beatSpike = 0;
      if (beatMarkerSamples > 0) { beatSpike = envelope * 1.2 + 20; beatMarkerSamples--; }
      float motionSpike = (fingerOn && millis() < motionHoldUntil) ? -(envelope * 0.5 + 20) : 0;
      Serial.printf("AC:%.1f\tSlope:%.1f\tTrig:%.1f\tBeat:%.1f\tMotion:%.1f\tGlow:%.0f\tBPM:%.0f\n",
                    fingerOn ? acSmooth  : 0.0f,
                    fingerOn ? slope     : 0.0f,
                    fingerOn ? slopeTrig : 0.0f,
                    beatSpike,
                    motionSpike,
                    glow,
                    bpmAvg);
#else
      if (beatMarkerSamples > 0) beatMarkerSamples--;
#endif
    }
  }

  // 2) The single render point, reached only once the FIFO is empty and
  //    all plotter output has been queued.
  renderStrip();
}
