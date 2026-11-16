# Smart Garage: CEDT Embedded Systems Final Project 2026

Miniature smart garage: a sliding door that opens for enrolled faces (on-device Face Recognition on ESP32-S3), bay occupancy sensing, automatic lighting, environmental logging to the cloud, a web dashboard and LINE alerts.

## Repository layout

```
gateway/        ESP32-S3 N16R8, PlatformIO + ESP-IDF
sensor-node/    ESP32 DOIT DevKit, PlatformIO + Arduino
shared/         UART protocol shared by both firmwares
cloud/          Google Apps Script (Sheets), MQTT / LINE notes
dashboard/      Web dashboard (Vercel)
docs/           Design handoff, proposal, protocol, pin map, test plan
```

## Getting started

1. Install VS Code + PlatformIO.
2. Open `gateway/` or `sensor-node/` as a PlatformIO project, then Build / Upload.
3. Copy `gateway/include/config.example.h` to `config.local.h` (git-ignored) and fill it in. **Never commit secrets.**

## Team

| Member | Role |
|---|---|
| TBD | System Architect & Integration |
| TBD | Embedded Developer (Sensor Node) |
| TBD | Embedded Developer (Gateway & Cloud) |
| TBD | UI/UX & Dashboard |
| TBD | Project Manager & Test Lead |

## Credits and AI tool declaration

- Third-party code and libraries: list here (e.g. ESP-WHO, Adafruit DHT).
- AI tools used: list tools and which parts they helped with (required by section 4.6).


## Dashboard / Gateway import

See [import and run notes](docs/LOCAL-IMPORT.md) for the imported dashboard, N16R8 Gateway, local configuration exclusions, and integration limitations.
