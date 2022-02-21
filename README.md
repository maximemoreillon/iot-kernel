# IOT Kernel
A base library for IoT devices.


## Features

* Configuration via web interface
* Configuration stored in SPIFFS
* Captive portal
* MQTT client

## Usage examples

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
