# IoT Kernel

A minimal starting point for IoT devices based on the ESP8266 or ESP32.

IoT Kernel takes care of what every device needs: connecting to WiFi, being configured from a web page, receiving firmware updates and connecting to an MQTT broker. Everything specific to a device, such as its state, the commands it accepts and the data it publishes, belongs in the sketch.

## Features

- WiFi connection, with an access point and captive portal as a fallback
- Configuration from a web page, saved in LittleFS
- OTA firmware updates and file upload
- Optional admin password protecting the web server and the access point
- MQTT connection, with availability (last will) and device info
- Callbacks for WiFi and MQTT connection events

## Dependencies

- [ESP8266](https://github.com/esp8266/Arduino) or [ESP32](https://github.com/espressif/arduino-esp32) integration in Arduino
- [PubSubClient](https://pubsubclient.knolleary.net/)
- [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) 3.x (ESP32Async) and its TCP library ([AsyncTCP](https://github.com/ESP32Async/AsyncTCP) or [ESPAsyncTCP](https://github.com/ESP32Async/ESPAsyncTCP))
- [ArduinoJson](https://arduinojson.org/) 7.x

Additionally, using this library involves static files that can be uploaded to ESP boards (LittleFS) using their respective filesystem uploaders.

## Flashing

1. Upload the GUI to the board using the LittleFS upload tool
2. Upload the sketch to the board as usual

## Usage

```cpp
#include "IotKernel.h"

IotKernel iot_kernel("device", "0.0.1");

void setup() {
  iot_kernel.init();
}

void loop() {
  iot_kernel.loop();
}
```

### MQTT

On connection, the device publishes `online` (retained) on `<username>/<device name>/availability`, with `offline` as its last will, and its type, nickname, firmware version and IP address on `<username>/<device name>/info`. It does not publish or subscribe to anything else on its own.

The sketch publishes and subscribes using `iot_kernel.mqtt` (a [PubSubClient](https://pubsubclient.knolleary.net/api)). `mqttBaseTopic`, `mqttStateTopic` and `mqttCommandTopic` provide conventional topic names, available after `init()`.

Subscriptions are lost when the connection drops, so they must be made in `onMqttConnect`, which runs on every connection:

```cpp
void mqtt_message_callback(char* topic, byte* payload, unsigned int length) {
  // NOTE: payload is not null-terminated
}

void setup() {
  iot_kernel.init();

  iot_kernel.mqtt.setCallback(mqtt_message_callback);

  iot_kernel.onMqttConnect([]() {
    iot_kernel.mqtt.subscribe(iot_kernel.mqttCommandTopic.c_str());
    // Also a good place to publish the current state, retained
  });
}
```

Publishing data, e.g. from a sensor:

```cpp
String topic = iot_kernel.mqttBaseTopic + "/temperature";
iot_kernel.mqtt.publish(topic.c_str(), "{\"temperature\":21.5}", MQTT_RETAIN);
```

### Connection callbacks

```cpp
iot_kernel.onWifiConnect([]() { /* ... */ });
iot_kernel.onWifiDisconnect([]() { /* ... */ });
iot_kernel.onMqttConnect([]() { /* ... */ });
iot_kernel.onMqttDisconnect([]() { /* ... */ });
```

### Web server

The sketch can add its own routes using `iot_kernel.http` (an [AsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer)). They are covered by the admin password and the cross-origin protection like the built-in ones.

| Route     | Method | Description                                                   |
| --------- | ------ | ------------------------------------------------------------- |
| /         | GET    | Settings page (files from the /www folder of LittleFS)        |
| /settings | POST   | Updates the settings, then reboots                            |
| /update   | GET    | Barebone page for OTA firmware updates                        |
| /update   | POST   | Uploads a new firmware file (.bin), as multipart/form-data    |
| /upload   | GET    | Barebone page for uploading files                             |
| /upload   | POST   | Uploads a file into /www, as multipart/form-data              |

## Security

- **Admin password**: when set in the settings form, the whole web server requires authentication (username `admin`), including routes added by the sketch. If it is at least 8 characters long, it is also used as the WiFi password of the access point, which is otherwise an open network. If the password is lost, re-upload `config.json` using the LittleFS upload tool.
- **Cross-origin requests**: requests other than GET and HEAD coming from another website are rejected, on every route.
- **Broker certificate**: with SSL enabled, the connection to the broker is encrypted but the broker's certificate is not verified.
- Password fields are never sent back to the browser: leave a field blank to keep the current password, or tick "Clear" to remove it.

## Notes

Providing an empty string as MQTT broker disables MQTT functions.

Leading and trailing spaces are removed from the text fields of the settings page (nickname, hostname, SSID, broker, username), but not from passwords.

If the configured WiFi cannot be reached at boot, the device starts an access point and retries the configured WiFi every 5 minutes while no client is connected to the access point.

## Migrating from 0.4 and earlier

The device no longer has a built-in state:

- `device_state`, `mqtt_publish_state()`, `onStateChange()` and the state argument of `init()` were removed. Keep the state in the sketch and publish it on `mqttStateTopic` (e.g. in `onMqttConnect` and whenever it changes).
- The device no longer subscribes to the command topic or handles `on`/`off` commands. Set a callback with `iot_kernel.mqtt.setCallback()` and subscribe in `onMqttConnect`.
- Public members and methods were renamed to camelCase, the Arduino convention:

  | 0.4                       | 1.0                     |
  | ------------------------- | ----------------------- |
  | `mqtt_base_topic`         | `mqttBaseTopic`         |
  | `mqtt_state_topic`        | `mqttStateTopic`        |
  | `mqtt_command_topic`      | `mqttCommandTopic`      |
  | `mqtt_info_topic`         | `mqttInfoTopic`         |
  | `mqtt_availability_topic` | `mqttAvailabilityTopic` |
  | `device_type`             | `deviceType`            |
  | `device_name`             | `deviceName`            |
  | `firmware_version`        | `firmwareVersion`       |

- `mqtt_publish_available()` and `mqtt_publish_info()` were removed: availability and info are published automatically on every connection.
- When `mqtt.broker.port` is missing from the configuration, port 1883 is used.
- ESPAsyncWebServer 3.x (ESP32Async) is required.
- Re-upload the GUI.
