Upload heartrate_scan normally, then close Arduino Serial Monitor/Plotter so the logger can use the USB port.

From PowerShell in this folder, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\log-serial.ps1 -Port COM7 -Seconds 60
```

The logger prints incoming output and saves it immediately in this folder's `logs` directory. No firmware change is needed. Change COM7 if the ESP32's port changes. Use `-Seconds 0` to capture until Ctrl+C. Close the logger before uploading again.

Ask Codex to read the latest heart-rate log after capturing. Logs contain all sensor readings and diagnostic messages received during capture, including serial corruption. The logger does not deliberately reset the ESP32; press EN/reset after starting it if startup messages are needed.
