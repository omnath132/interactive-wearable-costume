# Three independent LED outputs on ESP32-WROOM-32D

## What the hardware supports

GPIO16, GPIO17, and GPIO18 are separate output-capable pins on the
[ESP32-WROOM-32D module](https://documentation.espressif.com/esp32-wroom-32d_esp32-wroom-32u_datasheet_en.html).
Their UART/SPI alternate functions do not inherently couple them: peripheral
signals are assigned through the GPIO matrix. This target is the classic
ESP32, not an S3/C3 or a WROVER configuration using GPIO16/17 for PSRAM.

WS2812 LEDs need a precisely timed serial bitstream, not ordinary PWM.
[Espressif's RMT documentation](https://docs.espressif.com/projects/esp-idf/en/v5.1.5/esp32/api-reference/peripherals/rmt.html)
describes independent transmitter allocation by GPIO, concurrent operation,
buffer ownership during transmission, and underruns caused by interrupt
latency. The classic ESP32 does not provide RMT DMA.

[Espressif's LED driver notes](https://components.espressif.com/components/espressif/led_strip/versions/3.0.1/readme)
explicitly warn that delayed RMT interrupts, including Wi-Fi interrupts on
the same CPU core, can corrupt LED output. More RMT memory and less ISR
encoding work reduce this risk; separate GPIO numbers alone do not solve it.

## Implementation

The old NeoPixel output path reused a hardware transmitter as different strip
instances called show(). The first workaround detached GPIO routes manually.
Neither is used by the rebuilt driver. The reported interference has not been
measured with a logic analyzer, so its exact physical cause is not proven.

`IndependentRmtOutput.h` now:

- Allocates one native ESP-IDF RMT channel and copy encoder per GPIO at boot.
- Keeps all three channels allocated for the firmware's lifetime.
- Pre-encodes each frame into a separate internal-RAM symbol array.
- Waits for that channel's completion before reusing its symbol array; new
  animation frames are coalesced while a previous frame is in flight.
- Uses 128 symbols of hardware memory per channel and interrupt priority 3.
- Allocates the interrupts from setup on Core 1, away from default Wi-Fi on
  Core 0; the sketch rejects Arduino configurations that run on Core 0.
- Sends bits MSB-first in the buffer's configured order (GRB by default),
  with 0.4/0.8 microsecond zero pulses, 0.8/0.4 microsecond one pulses, and
  a 300 microsecond low reset at the end of every frame.
- Reports initialization/transmission errors instead of silently treating a
  failed output as working.

Adafruit NeoPixel is only a pixel buffer/color utility, constructed with pin
`-1`. No `begin()`, `show()`, or `setPin()` is called on that buffer.

## Hardware acceptance check

Upload the complete sketch folder and reload the page. Every panel should
show `fw=3ch-rmt-2`, `driver=dedicated-rmt`, `tx=ready`, and `tx_error=ESP_OK`.

1. Set strip 1 red, strip 2 green, and strip 3 blue. Each should retain its
   own color when the other two change.
2. Run red chase on strip 1 and green pulse on strip 2, leaving strip 3 blue.
3. Add rainbow on strip 3. Change only strip 2's brightness and direction.
4. Turn strip 2 off. Strips 1 and 3 should continue their own patterns.
5. Repeat while refreshing status over Wi-Fi. Frame counters should advance
   for animated strips without transmission errors.

Compilation and UI tests cannot verify electrical timing. If the issue
persists, distinguish identical frames on multiple pins from randomly
corrupted colors, and capture GPIO16/17/18 with a logic analyzer. That separates
routing/frame-ownership faults from timing underruns or signal integrity.
