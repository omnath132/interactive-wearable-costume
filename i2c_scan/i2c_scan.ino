/*
 * I2C bus scanner - ESP32, SDA = GPIO 21, SCL = GPIO 22
 *
 * Lists every device that answers on the bus, with a guess at what each
 * one is based on its address. Rescans every SCAN_MS so you can plug and
 * unplug things and watch the list change.
 *
 * Serial Monitor @ 115200.
 *
 * HOW IT WORKS
 *   Every I2C device has a 7-bit address. Addressing one and getting an
 *   ACK back means something is there; no ACK means nothing is. So we
 *   walk 0x01..0x7E and note which ones answer. No device-specific code
 *   is involved, which is why this works on anything.
 *
 * IF NOTHING IS FOUND
 *   - check VCC and GND to the sensor (3V3 on most breakouts)
 *   - check SDA/SCL are not swapped
 *   - both lines need pull-ups to 3V3. Most breakout boards have them
 *     fitted; a bare chip does not, and will look like an empty bus
 *   - a single bad jumper looks exactly like a dead sensor
 */

#include <Wire.h>

#define SDA_PIN   22
#define SCL_PIN   21
#define I2C_HZ    100000    // 100 kHz - the safe speed for scanning
#define SCAN_MS   3000      // rescan interval

// Common addresses, so the output says what a device probably is rather
// than just printing a number. Some addresses are shared by several
// parts, hence the "or".
const char* guessDevice(uint8_t addr) {
  switch (addr) {
    case 0x0D: return "HMC5883L magnetometer (alt)";
    case 0x18: return "LIS3DH accelerometer / MCP9808 temp";
    case 0x1E: return "HMC5883L / LSM303 magnetometer";
    case 0x20: return "PCF8574 or MCP23017 I/O expander";
    case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x25: case 0x26: case 0x27:
               return "PCF8574 / MCP23017 I/O expander (or LCD backpack)";
    case 0x29: return "TSL2561 light / VL53L0X distance";
    case 0x38: return "AHT10/AHT20 humidity or FT6206 touch";
    case 0x39: return "TSL2561 light sensor (alt)";
    case 0x3C: case 0x3D: return "SSD1306 / SH1106 OLED display";
    case 0x40: return "INA219 current / HTU21D / Si7021 humidity";
    case 0x44: case 0x45: return "SHT3x temperature+humidity";
    case 0x48: return "ADS1115 ADC / LM75 temperature";
    case 0x49: case 0x4A: case 0x4B: return "ADS1115 ADC (alt address)";
    case 0x50: case 0x51: case 0x52: case 0x53:
    case 0x54: case 0x55: case 0x56:
               return "AT24Cxx EEPROM";
    case 0x57: return "MAX30102/MAX30105 pulse sensor (or EEPROM)";
    case 0x5A: return "MLX90614 IR thermometer / CCS811 gas";
    case 0x68: return "MPU6050/MPU9250 IMU (or DS1307/DS3231 RTC)";
    case 0x69: return "MPU6050 IMU, AD0 pulled high";
    case 0x76: case 0x77: return "BMP280 / BME280 pressure+humidity";
    default:   return "unknown";
  }
}

void scan() {
  Serial.printf("\n--- scanning (SDA=%d, SCL=%d) ---\n", SDA_PIN, SCL_PIN);

  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();

    if (err == 0) {
      Serial.printf("  0x%02X  %s\n", addr, guessDevice(addr));
      found++;
    } else if (err == 4) {
      Serial.printf("  0x%02X  ERROR - device present but misbehaving\n", addr);
      found++;
    }
  }

  if (found == 0) {
    Serial.println("  nothing found.");
    Serial.println("  check power, check SDA/SCL are not swapped, and make");
    Serial.println("  sure both lines have pull-ups to 3V3.");
  } else {
    Serial.printf("  %d device(s) total\n", found);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=== I2C scanner ==="));
  Serial.println(F("Rescans every few seconds - plug things in and watch."));

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_HZ);
}

void loop() {
  scan();
  delay(SCAN_MS);
}
