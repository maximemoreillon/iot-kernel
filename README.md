# IOT Kernel

A library to handle basic functions for IoT devices. Currently supports ESP8266 and ESP32.

## Features

- WiFi
- Configuration via web interface
- Saving configuration in SPIFFS
- Generation of own access point if connection to desired wifi impossible
- Captive portal when in AP mode
- MQTT client
- OTA updates

# Dependencies

- [ESP8266](https://github.com/esp8266/Arduino) or [ESP32](https://github.com/espressif/arduino-esp32) integration in Arduino
- [PubSubClient](https://pubsubclient.knolleary.net/)
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [ArduinoJSON](https://arduinojson.org/)

Additionally, using this library involves static files that can be uploaded to ESP boards (LittleFS) using their respective filesystem uploaders

## Flashing

1. Upload the GUI to the board using the LittleFS upload tool
2. Upload the sketch to the board as usual

## REST API

Those are the endpoints of the Web server.

| Route   | Method | Description                                                           |
| ------- | ------ | --------------------------------------------------------------------- |
| /       | GET    | Accesses index.html file in SPIFFS                                    |
| /update | GET    | Barebone page for OTA firmware updates                                |
| /update | POST   | Endpoint to upload a new firmware file (.bin), as multipart/form-data |
| /upload | GET    | Barebone page for the updating the GUI                                |
| /upload | POST   | Endpoint to upload new GUI files, as multipart/form-data              |

## Notes

Providing an emtpy string as MQTT broker disables MQTT functions.
