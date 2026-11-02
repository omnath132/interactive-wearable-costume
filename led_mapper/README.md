# LED mapping sketch

Open `led_mapper.ino` in Arduino IDE and upload once. This standalone sketch replaces the running costume firmware while testing; it does not modify the costume controller source.

Use the same classic ESP32 / Arduino-ESP32 3.x setup as the costume controller, Arduino Runs On: Core 1. No extra Arduino libraries are required. Connect to **Costume-LED-Test**, then open **http://192.168.4.1**. This local test network has no password.

1. Choose an output. Pins are 16, 17, 32, 33, 4 (spine), 18. The other body labels are provisional.
2. Enter its strip length (1–256) and Save length. Defaults are guesses, not measurements.
3. Enter first and last LED numbers, inclusive. Both equal means one LED. LED 1 is closest to the strip data input. Use Previous/Next to walk along a strip, or Light whole strip to check the length.
4. Name each useful range, for example `Touch A electrode 0`, `Heart`, or `Left arm upper`, and Save selected range. A reused name replaces that record. Save up to 32 ranges across all outputs.
5. Download mapping JSON to keep a computer copy. Saved records can be loaded back into the selection and tested. Lengths and records persist in ESP32 flash; live illumination does not persist. Outputs start off at boot.

The touch names are manual records: this sketch does not read touch sensors or assign the ranges to the main controller automatically. JSON includes pins, lengths and inclusive ranges for transferring those measurements later. Shrinking a strip is blocked if a saved range would no longer fit. Delete that range first.

Brightness defaults to 16/255 and is capped at 64/255. Only the selected output lights; All LEDs off clears every output. A full 256-pixel frame is sent to each output so shortening a configured strip also clears previously illuminated pixels. Physically longer strips need a different limit and another upload. Keep the existing external LED power and common-ground arrangement; lengths and brightness determine power draw.

USB Serial Monitor at 115200 logs output initialization, tested ranges, saved lengths and records. Close Serial Monitor before uploading. The mapper uses one shared transmission buffer and sends outputs in sequence to keep RAM usage low. Physical LED behavior still needs testing on the ESP32.
