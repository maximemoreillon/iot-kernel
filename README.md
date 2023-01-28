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

## Examples

### Basic usage

```
#include "iotKernel.h"

IotKernel iot_kernel("device","0.0.1");

void setup() {
  iot_kernel.init();
}

void loop() {
 iot_kernel.loop();
}
```

### Adding web server endpoint

```
#include "iotKernel.h"

IotKernel iot_kernel("device","0.0.1");

void handleMyEndpoint(AsyncWebServerRequest *request) {
  request->send(200, "text/html", "My endpoint");
}

void setup() {
  iot_kernel.init();
  iot_kernel.http.on("/my-endpoint", HTTP_GET, handleMyEndpoint);
}

void loop() {
 iot_kernel.loop();
}
```

### Custom MQTT callback

```
#include "iotKernel.h"

IotKernel iot_kernel("device","0.0.1");

void mqtt_message_callback(char* topic, byte* payload, unsigned int payload_length) {
  Serial.print("MQTT messae received");
}

void setup() {
  iot_kernel.init();
  iot_kernel.mqtt.setCallback(mqtt_message_callback);
}

void loop() {
 iot_kernel.loop();
}
```
