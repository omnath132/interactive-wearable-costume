/*
 * MPU6050 gyro -> WS2812 "sudden stop" blink - ESP32
 *
 *   MPU6050: SDA = GPIO 21, SCL = GPIO 22, VCC -> 3V3, GND -> GND
 *   WS2812 : DATA = GPIO 23 (+ 5V and GND to the strip)
 *
 * Spin, swing or throw it, then STOP it abruptly - the whole strip
 * blinks. A gentle slowdown is ignored; only a fast halt counts.
 *
 * BOTH kinds of motion trigger it:
 *   ROTATION - from the gyro, in degrees/second
 *   LINEAR   - from the accelerometer, in g, with gravity removed
 * Whichever one stops hard fires the blink, so a motion that is part
 * spin and part shove still works.
 *
 * Library: Adafruit NeoPixel. The MPU6050 is driven directly over Wire,
 * no sensor library needed.
 * Serial Monitor / Plotter @ 115200.
 *
 * HOW THE DETECTION WORKS
 *   Each channel gets the same three-step test, on its own thresholds:
 *
 *     1. Value climbs above the MOVE threshold -> "moving", remember when
 *     2. Value falls below the STOP threshold  -> "stopped"
 *     3. If that took less than STOP_WINDOW ms, it was a SUDDEN stop
 *        -> blink. A slow coast down takes longer and is ignored.
 *
 *   REARM_MS after a trigger, neither channel will fire again - that
 *   stops it blinking twice for one event, and stops it retriggering on
 *   the wobble as you set the thing down.
 *
 *   One limitation worth knowing: the accelerometer measures
 *   ACCELERATION, not speed. Something sliding at a perfectly constant
 *   velocity reads the same as something sitting still. Real hand motion
 *   is never that smooth, so in practice this does not bite - but it is
 *   why "linear" here means "being accelerated", not "in motion".
 *
 * Serial Plotter traces (both channels scaled to % of their own MOVE
 * threshold, so they share one axis despite different units):
 *   Rot%   - rotation, 100 = at the ROT_MOVE threshold
 *   Acc%   - linear,   100 = at the ACC_MOVE threshold
 *   Move%  - the 100 line, for reference
 *   Fired  - spikes when a sudden stop is detected
 */

#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define PLOT        1       // 1 = stream plotter data, 0 = silent

#define SDA_PIN     22
#define SCL_PIN     21

// ---- LED strip ----
#define STRIP_PIN   23      // WS2812 data
#define STRIP_LEDS  18
#define COLOR_ORDER NEO_GRB
#define BRIGHTNESS  120     // master brightness, 0..255

// ---- blink look ----
#define BLINK_R     255     // blink colour
#define BLINK_G     255
#define BLINK_B     255
#define BLINK_TIMES   4     // how many on/off flashes per trigger
#define BLINK_MS     70     // ms per half-cycle (on, then off)

// ---- rotation detection (degrees/second) ----
#define ROT_MOVE    150     // above this = definitely rotating
#define ROT_STOP     30     // below this = definitely still

// ---- linear detection (g, with gravity removed) ----
#define ACC_MOVE   0.60     // above this = definitely being moved
#define ACC_STOP   0.15     // below this = definitely still

// ---- shared timing ----
#define STOP_WINDOW 200     // ms; faster than this from moving->stopped
                            //     counts as SUDDEN. Raise to catch softer
                            //     stops, lower to demand a harder one.
#define REARM_MS    600     // ignore new triggers for this long after one

// ---- pacing ----
#define POLL_MS      10     // gyro sample interval (100 Hz)
#define RENDER_MS    20     // ~50 fps, one show() per frame

Adafruit_NeoPixel strip(STRIP_LEDS, STRIP_PIN, COLOR_ORDER + NEO_KHZ800);

// ---- MPU6050 registers ----
#define MPU_ADDR       0x68   // 0x69 if AD0 is tied high
#define REG_PWR_MGMT_1 0x6B
#define REG_CONFIG     0x1A
#define REG_GYRO_CFG   0x1B
#define REG_ACCEL_CFG  0x1C
#define REG_ACCEL_XOUT 0x3B
#define REG_GYRO_XOUT  0x43
#define REG_WHO_AM_I   0x75   // reads 0x68 on a real MPU6050

// sensitivities for the ranges configured in configureSensor()
#define GYRO_LSB_PER_DPS 65.5     // +/-500 deg/s
#define ACCEL_LSB_PER_G  8192.0   // +/-4 g

// =====================================================================
// Detector state
//
// This block MUST stay above the first function definition in the file.
// The Arduino IDE auto-generates prototypes for every function and
// injects them just before the first one it finds - so any type used in
// a signature has to be declared before that point, or the generated
// prototype refers to a type the compiler has not seen yet.
// =====================================================================

// One of these per channel. The logic is identical for rotation and
// linear motion, only the thresholds and units differ.
struct Detector {
  bool  moving = false;          // currently above the move threshold
  unsigned long lastMovingMs = 0;// last time we were clearly moving
  float peak = 0;                // largest value seen during this movement
};

Detector rotDet;                 // rotation channel
Detector accDet;                 // linear channel

unsigned long rearmUntil = 0;    // shared: ignore triggers until this time

// =====================================================================
// I2C helpers
// =====================================================================
bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;   // repeated start
  if (Wire.requestFrom((uint8_t)MPU_ADDR, n) != n) return false;
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
                    a == MPU_ADDR ? "  <-- MPU6050 address" : "");
      found++;
    }
  }
  if (!found) Serial.println(F("    NOTHING FOUND - check VCC/GND/SDA/SCL wiring"));
  else        Serial.printf("    %d device(s) total\n", found);
  return found;
}

bool checkWhoAmI() {
  Serial.println(F("\n[2] Checking WHO_AM_I at 0x68..."));
  uint8_t id = readReg(REG_WHO_AM_I);
  Serial.printf("    WHO_AM_I = 0x%02X\n", id);
  if (id == 0x68) { Serial.println(F("    OK: MPU6050 confirmed")); return true; }
  if (id == 0x70 || id == 0x71 || id == 0x73) {
    Serial.println(F("    This is an MPU6500/9250 - close enough, continuing"));
    return true;
  }
  Serial.println(F("    Unexpected ID - wrong chip or bad communication"));
  return false;
}

void configureSensor() {
  writeReg(REG_PWR_MGMT_1, 0x80);          // reset
  delay(100);
  writeReg(REG_PWR_MGMT_1, 0x01);          // wake, clock from gyro X PLL
  delay(10);
  writeReg(REG_CONFIG,    0x03);           // DLPF ~44 Hz - kills hand tremor
  writeReg(REG_GYRO_CFG,  0x08);           // gyro range +/-500 deg/s
  writeReg(REG_ACCEL_CFG, 0x08);           // accel +/-4g (unused, but sane)
}

// Read both sensors in one burst. Accel, temperature and gyro sit in one
// contiguous block starting at 0x3B, so a single 14-byte read gets
// everything - fewer I2C transactions than two separate reads.
//
//   rot = rotation magnitude, degrees/second
//   acc = linear acceleration magnitude with gravity removed, g
//
// NOTE on `acc`: an accelerometer cannot tell acceleration from gravity,
// so at rest it reads 1g and we subtract that. The consequence is that
// this measures ACCELERATION, not velocity - something gliding at a
// constant speed reads zero, the same as sitting still. That is fine for
// catching a hard stop (deceleration is a big spike) and fine for any
// hand motion, which is never truly constant-velocity.
bool readMotion(float& rot, float& acc) {
  uint8_t d[14];
  if (!readRegs(REG_ACCEL_XOUT, d, 14)) return false;

  int16_t ax = (int16_t)(d[0]  << 8 | d[1]);
  int16_t ay = (int16_t)(d[2]  << 8 | d[3]);
  int16_t az = (int16_t)(d[4]  << 8 | d[5]);
  // d[6],d[7] are temperature - skipped
  int16_t gx = (int16_t)(d[8]  << 8 | d[9]);
  int16_t gy = (int16_t)(d[10] << 8 | d[11]);
  int16_t gz = (int16_t)(d[12] << 8 | d[13]);

  float rx = gx / GYRO_LSB_PER_DPS;
  float ry = gy / GYRO_LSB_PER_DPS;
  float rz = gz / GYRO_LSB_PER_DPS;
  rot = sqrtf(rx * rx + ry * ry + rz * rz);

  float lx = ax / ACCEL_LSB_PER_G;
  float ly = ay / ACCEL_LSB_PER_G;
  float lz = az / ACCEL_LSB_PER_G;
  // Magnitude is orientation-independent, so subtracting 1g works at any
  // tilt - no need to know which way is down.
  acc = fabsf(sqrtf(lx * lx + ly * ly + lz * lz) - 1.0f);
  return true;
}

// =====================================================================
// Sudden-stop detector - produces numbers only, touches no hardware
// =====================================================================
int   blinksLeft = 0;            // half-cycles remaining (on and off each)
unsigned long nextBlinkAt = 0;
bool  blinkOn = false;
int   firedMarker = 0;           // >0 -> Fired trace is spiking

void triggerBlink() {
  blinksLeft = BLINK_TIMES * 2;  // each flash is an on and an off
  nextBlinkAt = 0;               // fire the first half-cycle immediately
  blinkOn = false;               // renderStrip flips this before drawing
  firedMarker = 5;
}

// Shared "was moving, then suddenly wasn't" test. Returns true if this
// channel just detected a sudden stop.
bool feedDetector(Detector& d, float value, float moveThresh, float stopThresh,
                  const char* label, const char* units) {
  unsigned long now = millis();

  if (value > moveThresh) {
    d.moving = true;
    d.lastMovingMs = now;
    if (value > d.peak) d.peak = value;
    return false;
  }

  // Below the stop threshold, having been moving, and it happened fast.
  // A slow wind-down takes longer than STOP_WINDOW to get here, so the
  // elapsed check is what separates "stopped hard" from "coasted down".
  if (d.moving && value < stopThresh) {
    d.moving = false;
    float peak = d.peak;
    d.peak = 0;

    if (now - d.lastMovingMs <= STOP_WINDOW && now >= rearmUntil) {
      Serial.printf("STOP (%s): peak %.2f %s, halted in %lu ms\n",
                    label, peak, units, now - d.lastMovingMs);
      return true;
    }
  }
  return false;
}

// =====================================================================
// Renderer - the only code here that touches the strip
// =====================================================================
void renderStrip() {
  static unsigned long lastFrame = 0;
  static int lastValue = -1;

  unsigned long now = millis();
  if (now - lastFrame < RENDER_MS) return;
  lastFrame = now;

  // advance the blink sequence
  if (blinksLeft > 0 && now >= nextBlinkAt) {
    blinkOn = !blinkOn;
    blinksLeft--;
    nextBlinkAt = now + BLINK_MS;
  }
  if (blinksLeft == 0) blinkOn = false;    // always finish dark

  int v = blinkOn ? 1 : 0;
  if (v == lastValue) return;              // only push real changes
  lastValue = v;

  uint32_t c = blinkOn ? strip.Color(BLINK_R, BLINK_G, BLINK_B) : 0;
  for (int i = 0; i < STRIP_LEDS; i++) strip.setPixelColor(i, c);
  strip.show();
}

// =====================================================================
// Setup / loop
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("\n=== MPU6050 sudden-stop -> WS2812 blink ==="));

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  if (scanBus() == 0)  { Serial.println(F("\nFix wiring, then press reset.")); while (true) delay(1000); }
  if (!checkWhoAmI())  { Serial.println(F("\nStopping here.")); while (true) delay(1000); }

  configureSensor();

  Serial.println(F("\n[3] Ready - spin the gyro, then stop it suddenly.\n"));
}

void loop() {
  static unsigned long lastPoll = 0;

  if (millis() - lastPoll >= POLL_MS) {
    lastPoll = millis();

    float rot, acc;
    if (readMotion(rot, acc)) {
      // Either channel can fire. Both are evaluated every sample so a
      // motion that is part spin, part throw still registers whichever
      // one stops hard - but rearmUntil stops it blinking twice.
      bool stopped = false;
      if (feedDetector(rotDet, rot, ROT_MOVE, ROT_STOP, "rotation", "deg/s")) stopped = true;
      if (feedDetector(accDet, acc, ACC_MOVE, ACC_STOP, "linear",   "g"))     stopped = true;

      if (stopped) {
        triggerBlink();
        rearmUntil = millis() + REARM_MS;
      }

#if PLOT
      float fired = 0;
      if (firedMarker > 0) { fired = 100; firedMarker--; }
      // Rotation is scaled to a percentage of its own threshold so both
      // channels share one Y axis despite the different units.
      Serial.printf("Rot%%:%.0f\tAcc%%:%.0f\tMove%%:%d\tFired:%.0f\n",
                    rot / ROT_MOVE * 100.0,
                    acc / ACC_MOVE * 100.0,
                    100,
                    fired);
#else
      if (firedMarker > 0) firedMarker--;
#endif
    }
  }

  renderStrip();
}
