# Test and save sensor diagnostics

Revision **usb-debug-5** adds per-electrode stabilization: a three-sample median,
100 ms stable below 5 to press, and 400 ms stable at or above 6 to release.
Values of 5 retain the current state and interrupt pending transitions. Long
sampling gaps do not count toward stability time. Disconnects clear the filter
without generating release events. `TOUCH_STABILITY` shows the confirmed mask,
pending-transition bits and electrode-0 median. The timing constants live in
Config.h. These filters suppress brief fluctuations; physical held-touch behavior
still needs a fresh hardware test after uploading.

Revision **usb-debug-4** defaults to the requested rough touch detection:
an electrode must first read at least 8, then a filtered value below 5 counts
as touched; returning to 8 releases it (5–7 retains the previous state). `mask` is the software result, while
`hw` and `hw_th` describe the chip's separate touch mask/thresholds. The page
shows filtered values as well as baseline differences. Startup/stopped-sensor
failures are still faults; the rough rule cannot recover a chip that will not
initialize. No physical-touch fix is inferred just from successful host tests.

The thigh movement rule uses **0.15 g above resting magnitude plus 6 degrees
of gyro rotation within 800 ms**. Stand still for calibration. `IMU_RAW` reports
magnitude, gyro rates, recent rotation and threshold; accepted `STEP` entries
include the rotation. Rotation alone and acceleration alone do not count.

Revision **usb-debug-3** supports the observed MPU-6500 identity (0x70), as well
as MPU-6050. `SENSOR_INIT_FAILED` records the initialization phase;
`I2C_WRITE`/`I2C_READ_POINTER`/`I2C_READ_SHORT` record failed transactions, and
`TOUCH_RUN_CHECK` reports the enable register and status if the chip stops before
initialization completes. Touch B's captured zero baseline/out-of-range readings
remain a hardware/configuration diagnosis to resolve; support for the IMU's
correct identity does not fix that separate problem.

## USB debug revision usb-debug-2

Double-click **start-usb-log.cmd** to run a continuous COM7 recorder. While the
logger is running, it waits for USB and reconnects after unplug/replug, saving
data under `costume_system/logs`. Stop it with Ctrl+C before uploading or using
Arduino Serial Monitor. The ESP32 streams logs; the running computer-side
logger is what writes the file.

The firmware now uses IMU A on **mux 2 channel 2** and IMU B on **mux 2 channel 7**.
It probes addresses 0x68/0x69 on each, recording `IMU_PROBE` results (no address
acknowledgement, mux selection failure, or identity read failure) and `IMU_ID`
with the actual WHO_AM_I byte. An unsupported chip is reported explicitly.

`TOUCH_STATUS` includes the electrode enable register (expected ECR=0x8C),
filter/charge settings, electrode-0 thresholds and out-of-range status.
`TOUCH_RAW ... f/b/d 2=380/400/20` means electrode 2 has filtered reading 380,
baseline 400 and difference 20. All twelve electrodes are logged in three groups
per board once a second, even when the touch mask remains zero. Startup resets
each MPR121, checks its reset configuration and verifies all 12 electrodes are
enabled before marking it ready. These are diagnostic improvements, not proof
that a hardware detection problem has been resolved.

`HEART_RAW` includes red/IR readings, finger presence, filtered signal, estimated
BPM/period and accepted/rejected beat counts. Raw feedback continues even when
no new beat candidate is accepted. These fields also appear in Sensor monitor.

`RAW_DIAGNOSTIC_MS` in Config.h sets the raw-log interval (1000 ms by default;
zero disables it). The 64-event website history rolls off faster with raw logs,
so use USB capture for the full test. New diagnostics require uploading the
updated sketch. The startup `BOOT fw=usb-debug-2` line identifies this revision.

Register references: [MPR121 datasheet](https://www.nxp.com/docs/en/data-sheet/MPR121.pdf),
[Adafruit MPR121 driver](https://github.com/adafruit/Adafruit_MPR121/blob/master/Adafruit_MPR121.cpp).

Firmware logs go to USB serial at **115200 baud**, and the website's authenticated
**Diagnostic log** panel shows the latest 64 events. **Download recent log** saves
that history as a text file. Locked visitors cannot read `/api/logs`.

To save a complete test on your computer, close Arduino Serial Monitor/Plotter
and run this from the project folder in PowerShell, replacing COM7 if needed:

```powershell
powershell -ExecutionPolicy Bypass -File .\costume_system\log-serial.ps1 -Port COM7 -Seconds 120
```

Captures are saved under `costume_system/logs/costume-YYYYMMDD-HHMMSS-fff.log`.
`-Seconds 0` records until Ctrl+C. Start capture before your test; press the
board's reset button afterward if you also want startup/configuration messages.
Opening a serial port may reset some boards even though this script disables
DTR/RTS. Only one program can own the serial port at a time.

## Capacitive-touch check

1. Upload your configured sketch, start recording, and wait for `SENSOR_READY`
   for the touch board. Sensor IDs: **0 = touch A, 1 = touch B,
   2 = IMU A, 3 = IMU B, 4 = heart**. Mux labels in the log are
   one-based (mux 1/2); branch channels and electrodes are zero-based.
2. Touch one electrode, hold it for about a second, then release. Repeat for
   electrodes **0–11** on each board. Also try holding two electrodes together.
3. Look for one `TOUCH_DOWN` per pressed electrode and one `TOUCH_UP` on release.
   The release includes `held_ms`. A steady hold should not keep printing presses.
4. Check the electrode number and mask. `mask=0x005` means electrodes
   0 and 2 are both touched. `leds=[first,last)` is the mapped local LED range;
   it describes the software mapping, not evidence that LEDs physically lit.

Illustrative output, not a captured hardware result:

```text
[12 +8100 ms] SENSOR_READY sensor=0 mux=1 channel=6 address=0x5A
[13 +8150 ms] TOUCH_BASELINE source=physical arm=left mask=0x000
[14 +9200 ms] TOUCH_DOWN source=physical arm=left electrode=2 mask=0x004 leds=[6,10) override=0
[15 +10200 ms] TOUCH_UP source=physical arm=left electrode=2 held_ms=1000 mask=0x000 override=0
```

If initialization fails, `SENSOR_FAULT` identifies the mux, channel, address,
failure stage and accumulated fault count. A lost touch board produces
`TOUCH_LOST ... no_release_trigger`; reconnecting establishes a new baseline.
`override=1` means the section is manually controlled, so detection may work
without producing the normal sensor animation. Return the section to sensors
to test its animation.

## Important mode and storage details

The current configuration has **physical sensors enabled**. Bus 1 uses SDA/SCL
21/22 and bus 2 uses 26/25. Mux addresses and IMU branches are discovered;
`MUX_READY`, `IMU_FOUND`, `MUX_MISSING` and `IMU_MISSING` show the result. The
page's monitor shows live acceleration and registered step counts. Stay still
during the 50-sample warmup or press **Calibrate at rest** to restart it.

Touch A is configured for channel 6 and touch B for channel 7, both at 0x5A.
IMU A/B are channels 2/7; match the reported channels to actual placement.
The existing touch `arm=left/right` fields describe logical slots A/B, not a
verified physical side. LED mapping is not required to read/log electrodes,
and physical LEDs remain disabled. The isolated bring-up sketches are preserved.

Website simulation buttons log **`source=simulated`**; they cannot prove a physical
MPR121 is working. Actual acquisition logs **`source=physical`**. A sensor-ready
message confirms initialization, while touch edges confirm changing touch data.

The firmware keeps a bounded 64-event RAM history that is cleared on restart.
Older events roll off; the website reports the number overwritten. USB output is
drained in small chunks without waiting for serial buffer space. If production
outruns serial draining, `LOG_GAP` reports the gap. Capturing USB serial preserves
all events that actually reached the computer, not events already overwritten.

A five-second `STATUS` line includes presence bits (sensor IDs 0–4), both touch
masks, and LED-ready bits (left arm, right arm, left leg, right leg, spine, heart).
Step detections and accepted/rejected waveform beat candidates are also logged.
No passwords, session tokens, or password hashes are included in diagnostics.

After a test, share the saved log or point me to its path in this project and I
can check electrode transitions, missing releases, reconnections and faults.
