/*
 * Static wiring check: enable all mux ports, then hold SCL LOW, release SDA HIGH.
 * ESP32 SDA=21, SCL=22; TCA9548A address=0x70; Serial Monitor=115200.
 * No sensor reads, repeated scans, or clock pulses after setup.
 *
 * IMPORTANT: the mux is a switch, not a source of HIGH voltage. Each
 * downstream SDA/SCL branch needs pull-ups to its appropriate supply
 * (3.3 V for this setup; many sensor boards already have them).
 * ESP32 internal pull-ups are weak and do not replace branch pull-ups.
 * Measure SD0..SD7 and SC0..SC7 against common GND with a voltmeter.
 * Expect SD about 3.3 V and SC about 0 V after successful mux selection.
 * A HIGH voltage alone does not prove continuity: a local sensor pull-up
 * can make an isolated/disconnected branch read HIGH too.
 */
#include <Wire.h>

constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;
constexpr uint8_t MUX_ADDRESS = 0x70;
constexpr uint8_t PORT_MASK = 0xFF; // all eight; use (1u << 2) for only port 2
bool selectionVerified = false;

void releaseBus() {
  Wire.end();
  // Never use OUTPUT/HIGH on I2C: another device may be holding a line LOW.
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, INPUT_PULLUP);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nMUX static SDA HIGH / SCL LOW wiring check");
  if (!Wire.begin(SDA_PIN, SCL_PIN, 100000)) {
    Serial.println("ERROR: I2C initialization failed; mux selection is unknown.");
    releaseBus();
    return;
  }
  Wire.setTimeOut(20);
  Wire.beginTransmission(MUX_ADDRESS);
  Wire.write(PORT_MASK);
  uint8_t error = Wire.endTransmission();
  if (error != 0) {
    Serial.printf("ERROR: mux at 0x%02X did not accept mask (error %u).\n", MUX_ADDRESS, error);
  } else if (Wire.requestFrom(MUX_ADDRESS, static_cast<size_t>(1)) != 1) {
    Serial.println("ERROR: unable to verify mux selection.");
  } else {
    uint8_t actual = Wire.read();
    selectionVerified = actual == PORT_MASK;
    Serial.printf("Requested mask=0x%02X, readback=0x%02X: %s\n",
                  PORT_MASK, actual, selectionVerified ? "VERIFIED" : "MISMATCH");
  }
  releaseBus(); // preserves mux control register; stops ESP32 I2C traffic
  if (!selectionVerified) {
    Serial.println("Mux selection failed: leaving both lines released; static test not started.");
    return;
  }
  // Pull SCL down only; never actively drive an I2C line HIGH.
  digitalWrite(SCL_PIN, LOW);
  pinMode(SCL_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(SCL_PIN, LOW);
  Serial.println("SDA released HIGH with pull-ups; SCL held LOW. No more I2C traffic.");
  Serial.println("Measure SD0..SD7: about 3.3 V; SC0..SC7: about 0 V, relative to GND.");
  Serial.println("SDA HIGH alone does not prove continuity. Upload a sensor sketch to resume reads.");
}

void loop() {
  // Read only ESP32 input levels; this does not clock or address any sensor.
  Serial.printf("Mux selection: %s | ESP32 SDA=%s SCL=%s (not individual port readings)\n",
                selectionVerified ? "verified at setup" : "UNVERIFIED",
                digitalRead(SDA_PIN) ? "HIGH" : "LOW",
                digitalRead(SCL_PIN) ? "HIGH" : "LOW");
  delay(2000);
}
