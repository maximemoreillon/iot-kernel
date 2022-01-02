#include "iotKernel.h"



void IotKernel::MQTT_setup(){

  Serial.print("[MQTT] Setup with broker:  ");
  Serial.print(this->config.mqtt.broker.host);
  Serial.print(":");
  Serial.println(this->config.mqtt.broker.port);

  this->MQTT_client.setServer(this->config.mqtt.broker.host.c_str(), this->config.mqtt.broker.port);
  this->MQTT_client.setCallback([this](char* topic, byte* payload, unsigned int payload_length) { mqtt_message_callback(topic, payload, payload_length); });
}



String IotKernel::get_mqtt_base_topic(){
  return "/" + this->config.mqtt.username + "/" + this->get_device_name();
}

String IotKernel::get_mqtt_status_topic(){
  return this->get_mqtt_base_topic() + "/status";
}

String IotKernel::get_mqtt_command_topic(){
  return this->get_mqtt_base_topic() + "/command";
}


void IotKernel::MQTT_connection_manager(){

  static boolean last_connection_state = false;
  static long last_connection_attempt;

  if(this->MQTT_client.connected() != last_connection_state) {
    last_connection_state = this->MQTT_client.connected();

    if(this->MQTT_client.connected()){
      // Changed from disconnected to connected
      Serial.println("[MQTT] Connected");

      Serial.println("[MQTT] Subscribing to topic " + this->get_mqtt_command_topic());
      this->MQTT_client.subscribe(this->get_mqtt_command_topic().c_str());

      this->mqtt_publish_state();
    }
    else {
      // Changed from connected to disconnected
      Serial.print("[MQTT] Disconnected: ");
      Serial.println(this->MQTT_client.state());
    }
  }

  // Kind of similar to the pubsubclient example, one connection attempt every 5 seconds
  if(!this->MQTT_client.connected()){
    if(millis() - last_connection_attempt > MQTT_RECONNECT_PERIOD){
      last_connection_attempt = millis();

      // No need to do anything if not connected to WiFi
      if(!wifi_connected()) return;

      Serial.println("[MQTT] Connecting...");

      // Last will
      StaticJsonDocument<MQTT_MAX_PACKET_SIZE> outbound_JSON_message;

      outbound_JSON_message["connected"] = false;
      outbound_JSON_message["type"] = this->device_type;
      outbound_JSON_message["firmware_version"] = this->firmware_version;
      outbound_JSON_message["nickname"] = this->config.nickname;

      char mqtt_last_will[MQTT_MAX_PACKET_SIZE];
      serializeJson(outbound_JSON_message, mqtt_last_will, sizeof(mqtt_last_will));

//          Serial.println(this->get_device_name().c_str());
//          Serial.println(this->config.mqtt.username.c_str());
//          Serial.println(this->config.mqtt.password.c_str());
//          Serial.println(this->get_mqtt_status_topic().c_str());

      MQTT_client.connect(
        this->get_device_name().c_str(),
        this->config.mqtt.username.c_str(),
        this->config.mqtt.password.c_str(),
        this->get_mqtt_status_topic().c_str(),
        MQTT_QOS,
        MQTT_RETAIN,
        mqtt_last_will
      );
    }
  }

}

void IotKernel::mqtt_message_callback(char* topic, byte* payload, unsigned int payload_length) {

  Serial.print("[MQTT] message received on ");
  Serial.print(topic);
  Serial.print(", payload: ");
  for (int i = 0; i < payload_length; i++) Serial.print((char)payload[i]);
  Serial.println("");

  // Create a JSON object to hold the message
  // Note: size is limited by MQTT library
  StaticJsonDocument<MQTT_MAX_PACKET_SIZE> inbound_JSON_message;

  // Copy the message into the JSON object
  deserializeJson(inbound_JSON_message, payload);

  if(inbound_JSON_message.containsKey("state")){

    Serial.println("[MQTTT] Payload is JSON with state");

    // Check what the command is and act accordingly
    const char* command = inbound_JSON_message["state"];

    if( strcmp(command, "on") == 0 ) {
      this->device_state = "on";
      this->mqtt_publish_state();
    }
    else if( strcmp(command, "off") == 0 ) {
      this->device_state = "off";
      this->mqtt_publish_state();
    }

  }
  else {
    Serial.println("[MQTTT] Payload is NOT JSON with state");
    if(strncmp((char*) payload, "on", payload_length) == 0){
      this->device_state = "on";
      this->mqtt_publish_state();
    }
    else if(strncmp((char*) payload, "off", payload_length) == 0){
      this->device_state = "off";
      this->mqtt_publish_state();
    }
  }


}

void IotKernel::mqtt_publish_state(){
  Serial.println("[MQTT] State published");

  StaticJsonDocument<MQTT_MAX_PACKET_SIZE> outbound_JSON_message;

  outbound_JSON_message["connected"] = true;
  outbound_JSON_message["state"] = this->device_state;
  outbound_JSON_message["type"] = this->device_type;
  outbound_JSON_message["nickname"] = this->config.nickname;
  outbound_JSON_message["firmware_version"] = this->firmware_version;

  char mqtt_payload[MQTT_MAX_PACKET_SIZE];
  serializeJson(outbound_JSON_message, mqtt_payload, sizeof(mqtt_payload));
  this->MQTT_client.publish(get_mqtt_status_topic().c_str(), mqtt_payload, MQTT_RETAIN);

}
