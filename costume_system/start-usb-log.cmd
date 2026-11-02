@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0log-serial.ps1" -Port COM7 -Seconds 0 -Reconnect
