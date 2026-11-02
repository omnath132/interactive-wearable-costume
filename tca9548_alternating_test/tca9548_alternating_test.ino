/*
 * ESP32 + TCA9548A/CJMCU-9548 test. SDA=GPIO21, SCL=GPIO22.
 * CH2 / CH3: MPR121 capacitive touch, each at 0x5A.
 * CH7: MAX30102 heart sensor at 0x57, always enabled.
 * Masks 0x84/0x88 alternate: the two same-address touch boards are
 * never connected simultaneously. Channel numbers are labels 0..7.
 * Touch reads alternate every 20 ms (40 ms per board); heart FIFO is
 * polled every 5 ms. Serial 115200: touched electrodes and raw red/IR.
 * Filtered IR beats pulse the first WS2812 LED on LED_PIN with a soft fade.
 * Approximate BPM is visual feedback only; no SpO2 calculation.
 * Libraries: Adafruit MPR121 + BusIO + NeoPixel, built-in Wire.
 * Sensor models/settings from touch_led_blocks and max30102_test.
 */
#include <Wire.h>
#include <Adafruit_MPR121.h>
#include <Adafruit_NeoPixel.h>
#include "BeatFilter.h"

constexpr int SDA_PIN = 21; // matches touch_led_blocks and max30102_test
constexpr int SCL_PIN = 22; // matches touch_led_blocks and max30102_test
constexpr int LED_PIN = 16; // WS2812 data GPIO; change to your desired strip
constexpr uint16_t LED_COUNT = 1; // increase to clear the rest of a longer strip
constexpr uint8_t PULSE_BRIGHTNESS = 80; // 0..255
constexpr uint32_t FINGER_IR_THRESHOLD = 20000; // lower if a finger is not detected
Adafruit_NeoPixel pulseLed(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
BeatFilter beatFilter(FINGER_IR_THRESHOLD);
uint32_t beatCount = 0, pulseAt = 0, lastLedFrame = 0, lastSampleAt = 0;
bool pulseActive = false;

void resetBeatTracking() {
  beatFilter.reset();
  pulseActive = false;
}

void renderBeatLed() {
  uint32_t now = millis();
  if (now - lastLedFrame < 20) return;
  lastLedFrame = now;
  if (now - lastSampleAt > 500) resetBeatTracking();
  uint32_t age = now - pulseAt;
  uint8_t level = 0;
  if (pulseActive && age < 360) {
    level = age < 40 ? (age * 255 / 40) : ((360 - age) * 255 / 320);
  } else pulseActive = false;
  static int lastLevel = -1;
  if (lastLevel == level) return;
  lastLevel = level;
  pulseLed.clear();
  pulseLed.setPixelColor(0, pulseLed.Color(level, 0, 0));
  pulseLed.show(); // only render point; never inside FIFO draining
}
constexpr uint8_t MUX_ADDRESS = 0x70; // A0/A1/A2 low
constexpr uint8_t TOUCH_ADDRESS = 0x5A;
constexpr uint8_t HEART_ADDRESS = 0x57;
constexpr uint32_t POLL_MS = 5, TOUCH_MS = 20, REPORT_MS = 250;
Adafruit_MPR121 touch2, touch3;
bool busReady = false, heartReady = false, touchReady[2] = {false, false};
uint16_t touched[2] = {0, 0};
uint8_t nextTouch = 0;
uint32_t red = 0, ir = 0, samples = 0, heartErrors = 0, fifoOverflows = 0;
uint32_t touchErrors[2] = {0, 0}, muxErrors = 0;
uint32_t lastPoll = 0, lastTouch = 0, lastReport = 0;

bool readRegisters(uint8_t address, uint8_t reg, uint8_t* data, size_t length) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(address, length) != length) return false;
  for (size_t i = 0; i < length; i++) data[i] = Wire.read();
  return true;
}

bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg); Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool selectTouch(uint8_t index) {
  if (index > 1) return false;
  uint8_t mask = (1u << 7) | (1u << (index + 2));
  Wire.beginTransmission(MUX_ADDRESS);
  Wire.write(mask);
  if (Wire.endTransmission() != 0) { muxErrors++; return false; }
  // STOP applies the selection; verify before touching a same-address device.
  if (Wire.requestFrom(MUX_ADDRESS, static_cast<size_t>(1)) != 1 || Wire.read() != mask) {
    muxErrors++; return false;
  }
  return true;
}

bool initializeHeart() {
  uint8_t part;
  if (!readRegisters(HEART_ADDRESS, 0xFF, &part, 1) || part != 0x15) return false;
  if (!writeRegister(HEART_ADDRESS, 0x09, 0x40)) return false;
  bool resetDone = false;
  for (int attempt = 0; attempt < 100; attempt++) {
    uint8_t mode;
    if (!readRegisters(HEART_ADDRESS, 0x09, &mode, 1)) return false;
    if (!(mode & 0x40)) { resetDone = true; break; }
    delay(1); // initialization only
  }
  if (!resetDone) return false;
  // Existing max30102_test settings: 200 sps / averaging 8 = 25 FIFO samples/s.
  return writeRegister(HEART_ADDRESS, 0x08, 0x70) && // averaging 8, rollover
         writeRegister(HEART_ADDRESS, 0x04, 0) &&    // FIFO write pointer
         writeRegister(HEART_ADDRESS, 0x05, 0) &&    // overflow counter
         writeRegister(HEART_ADDRESS, 0x06, 0) &&    // FIFO read pointer
         writeRegister(HEART_ADDRESS, 0x0A, 0x2B) && // ADC 4096, 200 sps, 18-bit
         writeRegister(HEART_ADDRESS, 0x0C, 0x3C) && // red current
         writeRegister(HEART_ADDRESS, 0x0D, 0x3C) && // IR current
         writeRegister(HEART_ADDRESS, 0x09, 0x03);   // start red + IR mode
}

void readHeart() {
  if (!heartReady) return;
  uint8_t fifo[3]; // write pointer, overflow count, read pointer
  if (!readRegisters(HEART_ADDRESS, 0x04, fifo, 3)) { heartErrors++; return; }
  if (fifo[1] & 0x1F) {
    // Full/empty pointers are ambiguous after overrun; discard and restart FIFO.
    fifoOverflows++;
    resetBeatTracking();
    if (!writeRegister(HEART_ADDRESS, 0x04, 0) ||
        !writeRegister(HEART_ADDRESS, 0x05, 0) ||
        !writeRegister(HEART_ADDRESS, 0x06, 0)) heartErrors++;
    return;
  }
  uint8_t available = (fifo[0] - fifo[2]) & 0x1F;
  if (available && (millis() - lastSampleAt > 500 || available > 8)) resetBeatTracking();
  for (uint8_t i = 0; i < available; i++) {
    uint8_t data[6];
    if (!readRegisters(HEART_ADDRESS, 0x07, data, 6)) { heartErrors++; resetBeatTracking(); return; }
    red = ((uint32_t(data[0]) << 16) | (uint32_t(data[1]) << 8) | data[2]) & 0x3FFFF;
    ir = ((uint32_t(data[3]) << 16) | (uint32_t(data[4]) << 8) | data[5]) & 0x3FFFF;
    samples++;
    lastSampleAt = millis();
    if (beatFilter.feed(ir)) {
      beatCount++;
      pulseAt = millis();
      pulseActive = true;
    }
    if (!beatFilter.finger) pulseActive = false;
  }
}

void readTouch(uint8_t index) {
  if (!touchReady[index]) return;
  uint8_t status[2];
  if (!readRegisters(TOUCH_ADDRESS, 0x00, status, 2)) { touchErrors[index]++; return; }
  uint16_t state = (uint16_t(status[0]) | (uint16_t(status[1]) << 8)) & 0x0FFF;
  uint16_t changed = state ^ touched[index];
  for (uint8_t electrode = 0; electrode < 12; electrode++) {
    if (changed & (1u << electrode))
      Serial.printf("CH%u electrode %u: %s\n", index + 2, electrode,
                    state & (1u << electrode) ? "TOUCH" : "release");
  }
  touched[index] = state;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nMux test: MPR121 on CH2/CH3, MAX30102 always on CH7.");
  if (SDA_PIN < 0 || SCL_PIN < 0 || SDA_PIN == SCL_PIN) {
    Serial.println("Fill in distinct SDA_PIN and SCL_PIN GPIO numbers, then upload again.");
    return; // -1 must not silently select ESP32 default pins
  }
  if (LED_PIN == SDA_PIN || LED_PIN == SCL_PIN) {
    Serial.println("LED_PIN must differ from SDA_PIN and SCL_PIN."); return;
  }
  pulseLed.begin();
  pulseLed.setBrightness(PULSE_BRIGHTNESS);
  pulseLed.clear(); pulseLed.show();
  if (!Wire.begin(SDA_PIN, SCL_PIN, 100000)) {
    Serial.println("I2C initialization failed."); return;
  }
  Wire.setTimeOut(20);
  if (!selectTouch(0)) {
    Serial.println("Mux not responding at 0x70 or control readback failed. Reset after fixing."); return;
  }
  Serial.println("Keep fingers off touch pads during initialization.");
  touchReady[0] = touch2.begin(TOUCH_ADDRESS, &Wire);
  if (selectTouch(1)) touchReady[1] = touch3.begin(TOUCH_ADDRESS, &Wire);
  if (!selectTouch(0)) { Serial.println("Mux selection failed. Reset to retry."); return; }
  heartReady = initializeHeart();
  Serial.printf("CH2 MPR121: %s | CH3 MPR121: %s | CH7 MAX30102: %s\n",
                touchReady[0] ? "OK" : "FAILED", touchReady[1] ? "OK" : "FAILED",
                heartReady ? "OK" : "FAILED");
  Serial.println("Touch masks: bit 0 = electrode 0. Hold a finger still on MAX30102; allow 1-2 seconds to settle.");
  Serial.printf("Filtered beats pulse the first WS2812 LED on GPIO %d. BPM is approximate.\n", LED_PIN);
  Serial.println("Missing sensors do not stop the others. Reset after reconnecting a failed sensor.");
  busReady = true;
}

void loop() {
  if (!busReady) { delay(100); return; }
  uint32_t now = millis();
  if (now - lastPoll < POLL_MS) { renderBeatLed(); delay(1); return; }
  lastPoll = now;
  readHeart(); // CH7 is independent of the slower alternating touch schedule.
  if (now - lastTouch >= TOUCH_MS) {
    lastTouch = now;
    if (selectTouch(nextTouch)) readTouch(nextTouch);
    nextTouch ^= 1;
  }
  if (now - lastReport >= REPORT_MS) {
    lastReport = now;
    Serial.printf("CH2:0x%03X CH3:0x%03X | CH7 red:%lu IR:%lu samples:%lu | errors mux:%lu touch2:%lu touch3:%lu heart:%lu overflow:%lu\n",
                  touched[0], touched[1], (unsigned long)red, (unsigned long)ir,
                  (unsigned long)samples, (unsigned long)muxErrors,
                  (unsigned long)touchErrors[0], (unsigned long)touchErrors[1],
                  (unsigned long)heartErrors, (unsigned long)fifoOverflows);
    Serial.printf("Finger:%s BPM:%.1f beats:%lu filtered:%.1f\n",
                  beatFilter.finger ? "yes" : "no", beatFilter.bpm,
                  (unsigned long)beatCount, beatFilter.signal);
  }
  renderBeatLed();
}
