#include "IotKernel.h"

IotKernel iot_kernel("device","0.0.1");

void mqtt_message_callback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Message received on %s: %.*s\n", topic, (int) length, (char*) payload);
}

void setup() {
  iot_kernel.init();

  // Optional: receiving MQTT messages
  iot_kernel.mqtt.setCallback(mqtt_message_callback);

  // Subscriptions are lost on disconnect, so subscribe on every connection
  iot_kernel.onMqttConnect([]() {
    Serial.println("MQTT connected");
    iot_kernel.mqtt.subscribe(iot_kernel.mqtt_command_topic.c_str());
  });

  // Connection events
  iot_kernel.onMqttDisconnect([]() { Serial.println("MQTT disconnected"); });
  iot_kernel.onWifiConnect([]() { Serial.println("WiFi connected"); });
  iot_kernel.onWifiDisconnect([]() { Serial.println("WiFi disconnected"); });
}

void loop() {
 iot_kernel.loop();
}