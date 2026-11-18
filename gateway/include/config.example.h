#pragma once
// Copy to config.local.h. Never commit real credentials.
#define GARAGE_WIFI_SSID ""
#define GARAGE_WIFI_PASSWORD ""
// ESP32 uses TLS MQTT; browser uses a separate WSS endpoint on the SAME broker.
#define GARAGE_MQTT_URI ""
#define GARAGE_MQTT_USERNAME ""
#define GARAGE_MQTT_PASSWORD ""
#define GARAGE_DEVICE_ID "garage-01"
// Use an approved CA PEM here, or leave empty to use ESP-IDF certificate bundle.
#define GARAGE_MQTT_CA_PEM ""
#define GARAGE_SNTP_SERVER "pool.ntp.org"
// Optional parking HTTPS endpoint. Set both values in config.local.h.
#define GARAGE_PARKING_URL ""
#define GARAGE_PARKING_TOKEN ""
// Calibrate these distances for the installed sensor and bay.
#define GARAGE_PARKING_OCCUPIED_CM 60
#define GARAGE_PARKING_VACANT_CM 80
#define GARAGE_PARKING_STABLE_SAMPLES 3
