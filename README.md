# IOT Kernel

A base library for IoT devices.

## Features

- WiFi
- Configuration via web interface
- Configuration stored in SPIFFS
- Generation of own access point if connection to desired wifi impossible
- Captive portal when in AP mode
- MQTT client
- OTA updates

# Dependencies

- [PubSubClient](https://github.com/knolleary/pubsubclient/releases/tag/v2.8)
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)

Additionally, using this library involves static files that can be uploaded to ESP boards (LittleFS) using their respective filesystem uploaders

## Flashing

1. Upload the GUI to the board using the LittleFS upload tool
2. Upload the sketch to the board as usual
