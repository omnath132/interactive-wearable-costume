/*
 * ESP32 + TCA9548A/CJMCU-9548: scan all eight downstream ports.
 * SDA = GPIO21, SCL = GPIO22, Serial Monitor = 115200 baud.
 * Only Wire is required. Mux address defaults to 0x70.
 *
 * One port is enabled at a time, so identical sensor addresses on
 * different ports do not collide. All ports are disabled after scanning.
 * Names are address-based candidates, not confirmed device identities.
 * An empty scan means no ACK, not proof that nothing is physically plugged in.
 */
#include <Wire.h>

constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;
constexpr uint8_t MUX_ADDRESS = 0x70;
constexpr uint32_t SCAN_INTERVAL_MS = 5000;
bool busReady = false;
bool upstream[128] = {};

const char* deviceName(uint8_t address) {
  switch (address) {
    case 0x57: return "likely MAX30102/MAX30105 heart sensor; also EEPROM address";
    case 0x5A: return "likely MPR121 touch (your setup); also MLX90614/CCS811";
    case 0x5B: case 0x5C: case 0x5D: return "possible MPR121 touch (alternate address)";
    case 0x68: case 0x69: return "possible MPU6050/MPU9250 IMU or RTC";
    case 0x3C: case 0x3D: return "possible SSD1306/SH1106 OLED";
    case 0x76: case 0x77: return "possible BMP280/BME280 or another I2C mux";
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75:
      return "possible TCA9548A-family mux or other device";
    case 0x48: case 0x49: case 0x4A: case 0x4B: return "possible ADS1115 ADC / temperature sensor";
    case 0x29: return "possible VL53L0X/VL53L1X distance or light sensor";
    case 0x40: return "possible INA219 / HTU21D / Si7021";
    default: return "unidentified device (address alone is insufficient)";
  }
}

bool selectMask(uint8_t mask) {
  Wire.beginTransmission(MUX_ADDRESS);
  Wire.write(mask);
  uint8_t error = Wire.endTransmission(); // STOP makes selection take effect
  if (error != 0) {
    Serial.printf("  MUX ERROR: cannot write 0x%02X to 0x%02X (I2C error %u).\n",
                  mask, MUX_ADDRESS, error);
    return false;
  }
  if (Wire.requestFrom(MUX_ADDRESS, static_cast<size_t>(1)) != 1) {
    Serial.println("  MUX ERROR: control-register read failed.");
    return false;
  }
  uint8_t actual = Wire.read();
  if (actual != mask) {
    Serial.printf("  MUX ERROR: requested mask 0x%02X, received 0x%02X.\n", mask, actual);
    return false;
  }
  return true;
}

// Returns new downstream address count, or -1 for an incomplete/error scan.
int scanAddresses(bool baseline) {
  int count = 0;
  for (uint8_t address = 0x08; address <= 0x77; address++) {
    if (address == MUX_ADDRESS) continue;
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    if (error == 2) continue; // address NACK is normal for an absent device
    if (error != 0) {
      Serial.printf("  BUS ERROR %u probing 0x%02X; scan incomplete, not an empty port.\n", error, address);
      return -1;
    }
    if (baseline) upstream[address] = true;
    if (!baseline && upstream[address]) {
      Serial.printf("  0x%02X also present with all ports OFF; cannot assign it to this port.\n", address);
      continue;
    }
    Serial.printf("  0x%02X ACK - %s\n", address, deviceName(address));
    count++;
  }
  return count;
}

void scanMux() {
  Serial.println("\n========== ALL MUX PORTS 0..7 ==========");
  Serial.printf("SDA=%d SCL=%d mux=0x%02X\n", SDA_PIN, SCL_PIN, MUX_ADDRESS);
  if (!selectMask(0)) {
    Serial.println("Cannot isolate ports. Check mux address, power, ground, SDA/SCL and RESET.");
    Serial.println("If a downstream bus is stuck low, reset/power-cycle the mux before retrying.");
    return;
  }
  for (int i = 0; i < 128; i++) upstream[i] = false;
  Serial.println("Upstream bus (all ports OFF; mux itself excluded):");
  int baseline = scanAddresses(true);
  if (baseline < 0) return;
  if (baseline == 0) Serial.println("  No other upstream devices.");

  int counts[8];
  for (uint8_t port = 0; port < 8; port++) {
    Serial.printf("\nPORT %u:\n", port);
    if (!selectMask(1u << port)) {
      counts[port] = -1;
    } else {
      counts[port] = scanAddresses(false);
      if (counts[port] == 0) Serial.println("  No distinct downstream address responded.");
    }
    // Isolate a faulty port before attempting the next one.
    if (!selectMask(0)) {
      Serial.println("Cannot disconnect this port; remaining ports NOT TESTED.");
      Serial.println("Reset/power-cycle the mux and inspect the last selected branch.");
      return;
    }
  }
  Serial.println("\nSUMMARY (address-based detection):");
  for (uint8_t port = 0; port < 8; port++) {
    if (counts[port] < 0) Serial.printf("  PORT %u: ERROR / incomplete\n", port);
    else if (counts[port] == 0) Serial.printf("  PORT %u: no distinct device detected\n", port);
    else Serial.printf("  PORT %u: CONNECTED - %d responding address(es); see names above\n", port, counts[port]);
  }
  Serial.println("All ports now OFF. Repeating in 5 seconds.");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  busReady = Wire.begin(SDA_PIN, SCL_PIN, 100000);
  Wire.setTimeOut(20);
  if (!busReady) Serial.println("ERROR: I2C initialization failed.");
}

void loop() {
  if (busReady) scanMux();
  delay(SCAN_INTERVAL_MS);
}
