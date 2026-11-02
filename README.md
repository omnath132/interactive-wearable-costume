# Interactive Wearable Costume — Firmware

ESP32 firmware for an interactive wearable costume: heart rate, motion, and
touch input drive an addressable LED strip in real time. Each sketch here is
a standalone dev/bring-up test written while building out the full sensor
chain, kept as its own Arduino sketch so it can be opened and flashed
independently.

## Sketches

The new [integrated costume controller](costume_system/README.md) implements the
six-section body controls, effects, sensor simulation and owner/visitor access
from the handoff. The current configuration enables physical sensors and LED
outputs; review `costume_system/Config.h` against your wiring before uploading.

| Sketch | What it does |
|---|---|
| [`i2c_scan`](i2c_scan) | Scans the I2C bus over Serial and lists every device that answers, with a guess at what each one is. |
| [`heartrate_scan`](heartrate_scan) | MAX30102 live red/IR, waveform, and approximate BPM on a hosted page and Serial at 115200. Connect to Wi-Fi HeartMonitor (password heartbeat123), open http://192.168.4.1. SDA=21, SCL=22; mux 0x70 port 7 only. |
| [`tca9548_scan_all`](tca9548_scan_all) | Scans all eight mux ports individually, reports addresses and likely device names, and separates upstream devices from downstream results. SDA=21, SCL=22, mux=0x70; Serial at 115200. |
| [`tca9548_wiring_high`](tca9548_wiring_high) | Enables all mux ports, then stops I2C, releases SDA HIGH and holds SCL LOW for static voltage checks. Branch pull-ups are required; HIGH alone does not confirm continuity. SDA=21, SCL=22; Serial at 115200. |
| [`tca9548_alternating_test`](tca9548_alternating_test) | Alternates MPR121 touch boards on mux channels 2/3 while polling MAX30102 on channel 7. Filters IR beats and softly pulses the first WS2812 LED (GPIO 16 by default). SDA=21, SCL=22; configure LED_PIN and PULSE_BRIGHTNESS. Requires Adafruit MPR121, BusIO, and NeoPixel. Serial at 115200 reports approximate BPM and touch state. |
| [`i2c_scan_wifi`](i2c_scan_wifi) | Same I2C scan, but the ESP32 hosts its own Wi-Fi network and results page — no router or USB serial needed. |
| [`max30102_test`](max30102_test) | Drives the MAX30102 heart-rate sensor directly over I2C (no external library) and pulses a WS2812 strip in time with each beat. |
| [`heartbeat_60bpm`](heartbeat_60bpm) | Sensor-free control test: pulses the strip at a fixed BPM to isolate LED/wiring issues from sensor issues. |
| [`mpu6050_stop_blink`](mpu6050_stop_blink) | Reads the MPU6050 gyro + accelerometer and flashes the strip on a sudden stop after motion (spin, swing, or shove). |
| [`ws2812_wifi_ap`](ws2812_wifi_ap) | Three independent WS2812 channels on GPIO 16, 17, and 18, each supporting up to 181 LEDs, controlled through the ESP32's Wi-Fi captive portal. |

For the Wi-Fi LED controller, use the separate panels for strip 1 (GPIO 16),
strip 2 (GPIO 17), and strip 3 (GPIO 18). Each channel has its own color,
brightness, LED count, animation, and script. All three can animate at once.
In Serial Monitor, send `channel 1`, `channel 2`, or `channel 3` before the
usual commands; Serial defaults to channel 1. HTTP commands use
`/cmd?channel=2&c=fill%20red`; an explicit channel is required.
Manual color, power, or pattern commands stop the script on that strip only.
After uploading, reload the control page to get the three separate panels.
The rebuilt output driver shows `fw=3ch-rmt-2 driver=dedicated-rmt` in status.
Use a classic ESP32 / ESP32-WROOM-32D with Arduino-ESP32 3.x and **Arduino
Runs On: Core 1**. Keep `IndependentRmtOutput.h` beside the `.ino` file.
Adafruit NeoPixel remains a dependency for pixel storage and color math only;
its output driver is no longer used.

Each GPIO owns a persistent native RMT transmitter, encoder, and separate
frame snapshot. A snapshot is never modified while hardware is transmitting.
The three outputs can transmit concurrently, without changing GPIO routing.
Each transmitter reserves 128 hardware symbols (six of eight classic ESP32
memory blocks total), uses interrupt priority 3, and sends a 300 microsecond
low reset after every frame. Status reports `tx=ready`, `tx_error=ESP_OK`, and
a completed-frame counter separately for each strip.

Research and rationale are in [the driver notes](ws2812_wifi_ap/DRIVER_NOTES.md).

## Hardware

- **MCU:** ESP32
- **Sensors:** MAX30102 (heart rate, I2C), MPU6050 (gyro/accel, I2C), capacitive touch
- **Output:** WS2812 / NeoPixel addressable LED strip

## Status

The costume build is complete — see the [portfolio write-up](https://omnath132.github.io/#costume)
for photos, videos, and an interactive control-site demo. Host-side software
checks are separate from Arduino compilation, uploading, and physical hardware
validation; see the integrated controller's README for details.
