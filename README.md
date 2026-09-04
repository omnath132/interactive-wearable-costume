# Interactive Wearable Costume — Firmware

ESP32 firmware for an interactive wearable costume: heart rate, motion, and
touch input drive an addressable LED strip in real time. Each sketch here is
a standalone dev/bring-up test written while building out the full sensor
chain, kept as its own Arduino sketch so it can be opened and flashed
independently.

## Sketches

| Sketch | What it does |
|---|---|
| [`i2c_scan`](i2c_scan) | Scans the I2C bus over Serial and lists every device that answers, with a guess at what each one is. |
| [`i2c_scan_wifi`](i2c_scan_wifi) | Same I2C scan, but the ESP32 hosts its own Wi-Fi network and results page — no router or USB serial needed. |
| [`max30102_test`](max30102_test) | Drives the MAX30102 heart-rate sensor directly over I2C (no external library) and pulses a WS2812 strip in time with each beat. |
| [`heartbeat_60bpm`](heartbeat_60bpm) | Sensor-free control test: pulses the strip at a fixed BPM to isolate LED/wiring issues from sensor issues. |
| [`mpu6050_stop_blink`](mpu6050_stop_blink) | Reads the MPU6050 gyro + accelerometer and flashes the strip on a sudden stop after motion (spin, swing, or shove). |
| [`ws2812_wifi_ap`](ws2812_wifi_ap) | Standalone WS2812 controller: the ESP32 hosts its own Wi-Fi AP with a captive-portal control page for up to 181 LEDs. |

## Hardware

- **MCU:** ESP32
- **Sensors:** MAX30102 (heart rate, I2C), MPU6050 (gyro/accel, I2C), capacitive touch
- **Output:** WS2812 / NeoPixel addressable LED strip

## Status

Work in progress — see the [portfolio write-up](https://omnath132.github.io/) for the current state of the project.
