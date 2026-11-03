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

| Route     | Method | Description                                                           |
| --------- | ------ | --------------------------------------------------------------------- |
| /         | GET    | Accesses index.html file in SPIFFS                                    |
| /update   | GET    | Barebone page for OTA firmware updates                                |
| /update   | POST   | Endpoint to upload a new firmware file (.bin), as multipart/form-data |
| /upload   | GET    | Barebone page for the updating the GUI                                |
| /upload   | POST   | Endpoint to upload new GUI files, as multipart/form-data              |
| /settings | POST   | Endpoint to update the settings stored in SPIFFS                      |

## Usage

```cpp
IotKernel iot_kernel("device", "0.0.1");

void setup() {
  iot_kernel.init();

  // Called when the state is changed by an MQTT command
  iot_kernel.onStateChange([](const String& state) {
    // e.g. digitalWrite(RELAY_PIN, state == "on");
  });

  // Connection events
  iot_kernel.onWifiConnect([]() { /* ... */ });
  iot_kernel.onWifiDisconnect([]() { /* ... */ });
  iot_kernel.onMqttConnect([]() { /* e.g. publish extra topics */ });
  iot_kernel.onMqttDisconnect([]() { /* ... */ });
}

void loop() {
  iot_kernel.loop();
}
```

## Security

- **Admin password**: when set in the settings form, the web interface requires authentication (username `admin`). If it is at least 8 characters long, it is also used as the WiFi password of the access point, which is otherwise an open network. If the password is lost, re-upload `config.json` using the LittleFS upload tool.
- **Broker certificate**: with SSL enabled, the connection to the broker is encrypted but the broker's certificate is not verified.
- Password fields are never sent back to the browser: leave a field blank to keep the current password, or tick "Clear" to remove it.

## Notes

Providing an emtpy string as MQTT broker disables MQTT functions.

If the configured WiFi cannot be reached at boot, the device starts an access point and retries the configured WiFi every 5 minutes while no client is connected to the access point.
