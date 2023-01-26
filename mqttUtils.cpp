#include "IotKernel.h"



void IotKernel::mqtt_setup(){

  if(!this->config.mqtt.broker.host.length()) {
    Serial.println("[MQTT] Broker not provided, skipping MQTT config");
    return;
  }

  Serial.print("[MQTT] Connecting to broker: ");
  Serial.print(this->config.mqtt.broker.host);
  Serial.print(":");
  Serial.println(this->config.mqtt.broker.port);




  if(this->config.mqtt.broker.secure == "yes") {
    Serial.println("[MQTT] Connecting using SSL");
    this->mqtt.setClient(this->wifi_client_secure);
  }
  else {
    Serial.println("[MQTT] Connecting without SSL");
    this->mqtt.setClient(this->wifi_client);
  }


  this->mqtt.setServer(this->config.mqtt.broker.host.c_str(), this->config.mqtt.broker.port);
  this->mqtt.setCallback([this](char* topic, byte* payload, unsigned int payload_length) { mqtt_message_callback(topic, payload, payload_length); });

  // MQTT topics
  this->mqtt_base_topic = "/" + this->config.mqtt.username + "/" + this->device_name;
  this->mqtt_status_topic = this->mqtt_base_topic + "/status";
  this->mqtt_command_topic = this->mqtt_base_topic + "/command";
}




void IotKernel::mqtt_connection_manager(){

  static boolean last_connection_state = false;
  static long last_connection_attempt;

  if(this->mqtt.connected() != last_connection_state) {
    last_connection_state = this->mqtt.connected();

    if(this->mqtt.connected()){
      // Changed from disconnected to connected
      Serial.println("[MQTT] Connected");

      Serial.println("[MQTT] Subscribing to topic " + this->mqtt_command_topic);
      this->mqtt.subscribe(this->mqtt_command_topic.c_str());

      this->mqtt_publish_state();
    }
    else {
      // Changed from connected to disconnected
      Serial.print("[MQTT] Disconnected: ");
      Serial.println(this->mqtt.state());
    }
  }

  // Kind of similar to the pubsubclient example, one connection attempt every 5 seconds
  if(!this->mqtt.connected()){
    if(millis() - last_connection_attempt > MQTT_RECONNECT_PERIOD){
      last_connection_attempt = millis();

      // No need to do anything if not connected to WiFi
      if(!wifi_connected()) return;

      Serial.print("[MQTT] Connecting... (status: ");
      Serial.print(this->mqtt.state());
      Serial.println(")");

      // Last will
      StaticJsonDocument<MQTT_MAX_PACKET_SIZE> outbound_JSON_message;

      outbound_JSON_message["connected"] = false;
      outbound_JSON_message["type"] = this->device_type;
      outbound_JSON_message["version"] = this->firmware_version;
      outbound_JSON_message["nickname"] = this->config.nickname;

      char mqtt_last_will[MQTT_MAX_PACKET_SIZE];
      serializeJson(outbound_JSON_message, mqtt_last_will, sizeof(mqtt_last_will));


      this->mqtt.connect(
        this->device_name.c_str(),
        // this->get_hostname().c_str(),
        this->config.mqtt.username.c_str(),
        this->config.mqtt.password.c_str(),
        this->mqtt_status_topic.c_str(),
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
  outbound_JSON_message["version"] = this->firmware_version;
  outbound_JSON_message["address"] = WiFi.localIP().toString();

  char mqtt_payload[MQTT_MAX_PACKET_SIZE];
  serializeJson(outbound_JSON_message, mqtt_payload, sizeof(mqtt_payload));
  this->mqtt.publish(this->mqtt_status_topic.c_str(), mqtt_payload, MQTT_RETAIN);

}

void IotKernel::handle_mqtt(){
  if(this->config.mqtt.broker.host == "") return;
  this->mqtt_connection_manager();
  this->mqtt.loop();
}
