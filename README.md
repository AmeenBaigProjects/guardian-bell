# Guardian Bell MK1

GuardianBell MK1 is an ESP32-CAM based smart IoT doorbell and surveillance system featuring motion-activated monitoring, intelligent alerts, and scheduled cloud uploads. It supports deep sleep optimization, multiple wake modes, MQTT and Telegram notifications, Home Assistant integration, and OTA firmware updates.

## OTA Update System

Firmware updates are delivered using GitHub Releases.

The device checks:

- `version.txt` → latest firmware version  
- `firmware.bin` → compiled firmware binary  
- `update_notes.txt` → release notes  

If a newer version is detected, the firmware is downloaded,
flashed, and the device restarts automatically.