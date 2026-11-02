# Integrated costume controller

Open `costume_system.ino` with every header in this folder alongside it.
Target: **classic ESP32, Arduino-ESP32 3.x, Arduino Runs On: Core 1**.
Built-in WiFi, WebServer, DNSServer, Preferences and ESP-IDF libraries are used;
no additional Arduino sensor libraries are required.

The controller now starts with **physical sensors and LED outputs enabled**.
Measured lengths and touch regions are saved in `measured-led-mapping.json`.

| Section | GPIO | LEDs |
|---|---:|---:|
| Left arm | 16 | 118 |
| Right arm | 17 | 124 |
| Left leg | 32 | 70 |
| Right leg | 33 | 70 |
| Spine | 4 | 97 |
| Heart | 18 | 18 |

Touch channel 6 is left; channel 7 is right (user confirmed). The mapper's
1-based inclusive ranges have been converted to zero-based `[first,last)` ranges.
Left electrodes 0–4, 7, 10, 11 and right electrodes 0–4, 9–11 are mapped.
Other electrodes remain visible in diagnostics but do not light a region or
launch a release pulse. All strip indices currently follow data-input order;
physical effect direction and spine junction locations still need a costume test.

I2C buses use SDA/SCL **21/22** and **26/25**. IMUs use bus 2 channels 2 and 7;
physical left/right IMU mounting is still unassigned. Touch presses below 5 for
100 ms and releases at 6 or above for 400 ms, with median filtering.

## First use

1. Compile and upload with your normal Arduino setup.
2. Open Serial Monitor at **115200**, with newline enabled. Send
   `password YOUR_PASSWORD`, using at least eight characters. This USB command
   also resets access if you forget the password. Passwords are not printed.
3. Join the open Wi-Fi network **Costume**, then open
   **http://192.168.4.1**. The captive DNS redirect is provided as a convenience.
4. Sign in and watch **Sensor monitor**. It displays discovered mux addresses,
   touch electrodes, IMU channels/addresses, live acceleration, step counts and
   time since the last step. No controller claim is needed just to monitor.
5. Choose **Take control** for calibration/sensitivity changes. Stand still for
   the 50-sample warmup, then walk and watch the step counts and diagnostic log.
6. Open visitor access to let another device enter. It must claim control;
   other visitors can view. The owner can take over or lock visitor access.

Physical I2C and LED outputs are active. The diagram uses measured pixel counts;
arm routes join the top 12 spine LEDs. Setting
`ENABLE_SENSORS=false` explicitly restores simulation and its event buttons.

## Physical sensor connections and discovery

| Bus | SDA / SCL | Sensor branches |
|---|---|---|
| 1 | 21 / 22 | MPR121 at 0x5A on channels 6 and 7; MAX30102 at 0x57 on channel 2 |
| 2 | 26 / 25 | IMU A on channel 2, IMU B on channel 7; probe addresses 0x68/0x69 |

Each bus probes for one responding mux at 0x70â€“0x77. `MUX_ADDRESS` can pin each
address instead. Both muxes may use 0x70 because they are on separate controllers.
Only one downstream branch is enabled at a time on each mux. This setup assumes
one mux per controller; it is not intended to discover arbitrary bus topologies.

Touch channels/addresses above are defaults based on the intended arrangement;
edit `TOUCH` if your wiring differs. IMUs must pass the WHO_AM_I check before
assignment. A/B are fixed to user-specified channels 2/7, with addresses probed
separately on each channel. `IMU_PROBE` reports mux/ACK/read failures and `IMU_ID`
reports the actual identity value, including unsupported devices. Setting a
`MOTION` channel to -1 enables a full channel search for that slot if needed.
Touch A/B identify channels 6/7: left/right arms respectively.

Missing sensors retry independently; a missing heart or second mux does not stop
working touch sensors. Electrode-to-LED mapping is not required for monitoring.
The page refreshes about twice a second; sensor sampling runs independently.

Sensor references: [ESP32 I2C API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html),
[MPU6050 register map](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map.pdf).

## Behavior and implementation defaults

- Six independently controlled sections: both arms, both legs, spine, heart.
  Logical limb index zero is the distal end. Logical spine zero is its bottom.
  The output adapter applies physical reversal after mixing.
- Holds charge each electrode's mapped region independently, reaching full glow
  after 2.5 seconds. Only release launches a traveling blue pulse; longer holds
  produce brighter, wider pulses. The first valid touch sample establishes state. Failed/disconnected
  reads clear held regions without manufacturing release events.
- Up to 24 pulses travel independently at 35 pixels/second, with a soft radius
  from five to twelve pixels depending on charge. Pulses from different limbs
  meeting on the spine trigger a 320 ms white flare, once per pair. Eight flare
  slots bound memory use; overflow is counted with dropped effects. Saturating
  blending caps each color component at 160. New events
  are dropped and counted when the pool is full.
- Sensor-controlled arms, legs and spine have a dim blue breathing background
  with a 4.5-second cycle. Manual sections replace that background; the heart
  keeps its red heartbeat behavior.
- **Lightning** adds two brief white flashes over 700 ms. **Energy wave** sweeps
  blue light from limbs onto the spine over 2.2 seconds. **Heart restart** blacks
  out the suit for 900 ms, shows two red heart double-pulses, then fades the blue
  background back in, finishing after 2.4 seconds. These global one-shots cover
  manual sections temporarily without changing their ownership/settings or the
  heart scheduler. A new one-shot replaces the previous one; **Stop effect** or
  **Return all to sensors** cancels it. Hardware sampling continues throughout.
- A first usable beat immediately flashes the heart; a smaller second flash
  follows after 180 ms. Before beats, the baseline is dim red. Prediction starts
  after five accepted beats; the period averages their four intervals. It
  continues indefinitely, with status changing from live to predicted after
  2.5 seconds without an accepted detection.
- Intervals outside 333â€“1714 ms are rejected. Established-tempo deviations over
  35% do not update timing or flash. Three consistent changed-tempo intervals
  start a fresh history, allowing tempo reacquisition. Loss of waveform tracking
  clears the detection anchor while retaining the prediction period.
- A real beat within 140 ms after a visual flash reanchors timing without a
  duplicate. FIFO sample time drives estimation; delivery time drives the flash.
  Late service coalesces predictions rather than replaying missed beats.
- Painting switches that section to manual mode. Brush commands interpolate
  the drag path, default to ten pixels wide, and clamp to the section. Persistent
  paint lasts until cleared; Fade strokes decay over 2.5 seconds. Section
  brightness also scales painting. Brush width one supports individual pixels.
- Manual sections suppress sensor rendering locally. A manual spine suppresses
  its part of a route; the limb still runs. Sensors continue sampling during
  overrides. Returning a section to sensors clears its paint and relevant
  traveling pulses, so old triggers do not replay.
- Thigh-mounted step detection combines **0.15 g above the learned resting
  magnitude** with **6 degrees of net gyro rotation** within a 800 ms window.
  Either condition may arrive first. A 350 ms step lockout and fresh rotation
  after each accepted step prevent immediate repeats. Calibration learns gyro
  bias and resting acceleration over 50 samples; stand still. Website sensitivity
  adjusts the acceleration threshold. Rotation and gyro readings are visible in
  Sensor monitor, `IMU_RAW` logs and accepted `STEP` logs. Sampling gaps discard
  accumulated motion instead of inventing rotation.
  These defaults require walking tests at the actual mounting points.

These tuning values are implementation defaults, not confirmed hardware facts.

## Access and recovery choices

The website starts **locked after every reboot**, with no controller. Visitor
access, sessions, overrides, painting and sensitivity are held only in RAM.
Only the salted SHA-256 owner password hash and salt persist in Preferences.
Physical USB access can replace that password and revoke all sessions.

Sessions use random 128-bit tokens, HttpOnly/SameSite cookies, one-hour idle
expiry and a maximum of eight sessions. A valid owner login can reclaim a slot
when the table is full. A controller lease lasts 30 seconds; an active visible
page renews it once per second. Closing the page releases control after expiry.
Owner takeover is immediate. Closing visitor access revokes visitor sessions.

State endpoints enforce access; every mutation requires a custom session header,
and lighting commands additionally require current controller ownership. The
public page initially shows only access controls. No live pixels or sensor data
are fetched for a locked visitor. No owner credential appears in page assets.
Failed login attempts are paced, with a 30-second cooldown after five failures.

The open access point and HTTP **do not encrypt passwords, sessions or commands**.
Use a unique password for this costume. The stored hash is not a slow password
KDF; this is local event access control, not a hardened Internet-facing service.

## Hardware output and checking the mapping

Use classic ESP32 and WS2812-compatible **800 kHz, GRB** strips. Compile with
**Partition Scheme: Huge APP** and **Arduino Runs On: Core 1**. Upload the main
`costume_system.ino`, then connect to **Costume** and open **http://192.168.4.1**.
Use your existing website password, or set/recover it with the USB command above.
LEDs start off; heart beats, steps, and mapped touch
holds/releases drive the corresponding effects. Website manual controls override
sensor effects for that section; return the section to sensor mode to test touch.

Check each section using a static color, then return to sensor mode and test
holds/releases and walking. USB logs report LED initialization, sensor discovery,
touch edges and unmapped releases. Six-output behavior under Wi-Fi and physical
effect directions still need testing on the actual costume. External LED power
and a common ground with the ESP32 are required.

Each output retains its own RMT transmitter and encoder (64 hardware symbols).
One shared 11.9 KB symbol buffer is reused after each transmission completes.
Completion is polled without blocking sensor reads or web requests,
avoiding six large RAM buffers. Frames pad to 124 pixels and keep unused pixels
black. If future strips exceed 124 pixels, increase `MAX_PIXELS` and review RAM.

RMT reference: [Espressif ESP32 RMT documentation](https://docs.espressif.com/projects/esp-idf/en/v5.1.5/esp32/api-reference/peripherals/rmt.html).

Sensor work is round-robin, one branch per 10 ms service opportunity, with
incremental initialization, five-millisecond I2C transaction timeouts, bounded
FIFO draining (two samples per heart visit), and three-second reconnect delays.
Heart FIFO overflow/backlog restarts acquisition; persistent missing samples
report a fault. No separate sensor RTOS tasks are used.

The standard synchronous WebServer can delay the loop while receiving/sending
requests. The animation and sensor schedulers avoid deliberate long waits, but
this is not a hard real-time latency guarantee. Validate sustained simultaneous
operation before wearing it.

## Software checks

The default touch rule now uses the electrode's filtered value directly: observe
at least 6 to arm it, register touch below 5, and release at 6 or above. A
three-sample median rejects isolated spikes. Press needs 100 ms of stable low
readings; release needs 400 ms of stable high readings, preventing brief upward
fluctuations from ending a hold. Values of 5 retain the previous state and cancel
pending transitions. `USE_ROUGH_TOUCH=false` restores the MPR121's own
touch mask. Filtered readings are polled on every touch visit (about 50 ms),
independently of the one-second diagnostic logging interval. A failed sensor
must reconnect and show a normal value again before this rule can trigger.

Sensor events, connection faults and five-second status summaries are logged to
USB serial and a protected website log view. See [LOGGING.md](LOGGING.md) for
electrode testing and saving a complete test to your computer.

- `node tests/costume-web.cjs` runs the actual embedded page script with a mock
  device and checks locked access, sign-in, section commands, release events,
  interpolated brush commands and access revocation.
- Compile/run `tests/costume-engine.cpp` with a C++17 host compiler. It exercises
  touch disconnects, pulse routing, override isolation, paint/fade, overflow,
  heartbeat startup/prediction/reconciliation/tempo changes/rollover, controller
  ownership/revocation/expiry, step retrigger behavior, independent charging,
  ghost breathing, collision de-duplication/pool reuse, and one-shot restoration.
- `tests/costume-sensors.cpp` uses a fake `Wire` bus (include `tests/fakes`) to
  check independent controllers, duplicate mux addresses across buses, IMU
  identity/discovery, touch transitions, step detection and fault recovery.
- Optional `node tests/costume-browser.cjs` uses Playwright to check the actual
  mobile page layout and interactions with a mock device. `COSTUME_BROWSER` can
  select an installed Chrome executable. The screenshot is ignored by Git.

These checks do not validate Arduino compilation, physical waveforms, electrical
health or walking sensitivity. Compilation and uploading remain with you, as
specified in the handoff.

Sensor pulses travel toward decreasing physical LED indices. Step pulses start
at the far end of each leg. The spine has no idle breathing glow; each pulse
enters it only after its center passes below limb LED 0. Explicit spine manual
controls and one-shot effects can still light the spine.

Idle LEDs are off, including the heart between beats. Sensor effects use deep
blue. Arm pulses enter spine index 85 (mapper LED 86, first of the top 12 LEDs)
and travel toward index 96. Collisions tint yellow for 320 ms before returning
to blue. Manual controls and explicit one-shot effects remain available.

The website has a **Faint blue at rest** toggle after claiming control. Off is
the initial default; your choice is saved through restarts. Faint blue applies
to arms and legs, keeping the spine dark until a pulse arrives and the heart
dark between beats. Manual overrides take priority over the idle setting.

## Local body animation

The ESP32 sends a compact animation snapshot about five times per second:
controller uptime, pulse origins and timestamps, touch hold times, heartbeat
phase, collision flares, one-shot timing, strip lengths and junctions. The
browser renders colors at its own display refresh rate and compensates for
request travel time. Pixel color arrays are no longer sent for ordinary frames;
manual paint uses sparse saved strokes. Sensor diagnostics refresh independently.

The picture uses physical LED indices: limb LED 0 meets the spine, arm links meet
spine index 85, and leg links meet index 0. Hit testing uses the same reversed
limb routes, so painting and moving pulses agree. SVG dots are reused rather than
rebuilt every poll. An update gap over 1.5 seconds freezes extrapolation and
shows an updates-paused indicator. Reconnecting gets authoritative current state,
including in-progress pulses. The visual represents commanded output; it does
not measure the physical light or wireless display latency.

Footstep pulses use half the previous spatial width, on both legs and spine.
Their travel speed, brightness and touch-pulse widths are unchanged. The radius
is included in animation state so the browser uses the same size.
