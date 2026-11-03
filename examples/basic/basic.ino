#include "IotKernel.h"

IotKernel iot_kernel("device","0.0.1");

void setup() {
  iot_kernel.init();

  // Called when the state is changed by an MQTT command
  iot_kernel.onStateChange([](const String& state) {
    Serial.printf("New state: %s\n", state.c_str());
  });

  // Connection events
  iot_kernel.onWifiConnect([]() { Serial.println("WiFi connected"); });
  iot_kernel.onWifiDisconnect([]() { Serial.println("WiFi disconnected"); });
  iot_kernel.onMqttConnect([]() { Serial.println("MQTT connected"); });
  iot_kernel.onMqttDisconnect([]() { Serial.println("MQTT disconnected"); });
}

void loop() {
 iot_kernel.loop();
}