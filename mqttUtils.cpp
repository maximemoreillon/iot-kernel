#include "IotKernel.h"


void IotKernel::mqtt_setup(){

  if(this->is_unset(this->config.mqtt.broker.host)) {
    Serial.println("[MQTT] Broker not provided, MQTT disabled");
    return;
  }

  Serial.printf("[MQTT] Connecting to broker %s:%u\n",this->config.mqtt.broker.host.c_str(),this->config.mqtt.broker.port);


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
  this->mqtt_base_topic = this->config.mqtt.username + "/" + this->device_name;
  this->mqtt_state_topic = this->mqtt_base_topic + "/state";
  this->mqtt_command_topic = this->mqtt_base_topic + "/command";
  this->mqtt_availability_topic = this->mqtt_base_topic + "/availability";
  this->mqtt_info_topic = this->mqtt_base_topic + "/info";
}




void IotKernel::mqtt_connection_manager(){

  static boolean last_connection_state = false;
  static long last_connection_attempt;

  if(this->mqtt.connected() != last_connection_state) {
    last_connection_state = this->mqtt.connected();

    if(this->mqtt.connected()){
      // Changed from disconnected to connected
      Serial.println("[MQTT] Connected");

      Serial.printf("[MQTT] Subscribing to topic %s\n", this->mqtt_command_topic.c_str());
      this->mqtt.subscribe(this->mqtt_command_topic.c_str());

      this->mqtt_publish_state();
      this->mqtt_publish_available();
      this->mqtt_publish_info();
    }
    else {
      // Changed from connected to disconnected
      Serial.printf("[MQTT] Disconnected (state: %d)\n", this->mqtt.state());
    }
  }

  // Kind of similar to the pubsubclient example, one connection attempt every 5 seconds
  if(!this->mqtt.connected()){
    if(millis() - last_connection_attempt > MQTT_RECONNECT_PERIOD){
      last_connection_attempt = millis();

      // No need to do anything if not connected to WiFi
      if(!wifi_connected()) return;

      Serial.printf("[MQTT] Connecting... (state: %d)\n",this->mqtt.state());


      // Last will
      JsonDocument outbound_JSON_message;

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
        // Last will
        this->mqtt_availability_topic.c_str(),
        MQTT_QOS,
        MQTT_RETAIN,
        "offline"
      );
    }
  }

}


void IotKernel::mqtt_message_callback(char* topic, byte* payload, unsigned int payload_length) {

  Serial.print("[MQTT] Message received on ");
  Serial.print(topic);
  Serial.print(", payload: ");
  for (int i = 0; i < payload_length; i++) Serial.print((char)payload[i]);
  Serial.println("");

  // Create a JSON object to hold the message
  // Note: size is limited by MQTT library
  JsonDocument inbound_JSON_message;

  // Copy the message into the JSON object
  deserializeJson(inbound_JSON_message, payload);
  
  if(inbound_JSON_message.containsKey("state")){
    char* command = strdup(inbound_JSON_message["state"]);
    if( strcmp(strlwr(command), "on") == 0 ) {
      this->device_state = "on";
      this->mqtt_publish_state();
    }
    else if( strcmp(strlwr(command), "off") == 0 ) {
      this->device_state = "off";
      this->mqtt_publish_state();
    }
    free(command);
  }
  else {
    if( strncmp(strlwr((char*) payload), "on", payload_length) == 0 ) {
      this->device_state = "on";
      this->mqtt_publish_state();
    }
    else if( strncmp(strlwr((char*) payload), "off", payload_length) == 0 ) {
      this->device_state = "off";
      this->mqtt_publish_state();
    }
  }

}

void IotKernel::mqtt_publish_state(){
  
  JsonDocument outbound_JSON_message;
  
  outbound_JSON_message["state"] = this->device_state;
  
  char mqtt_payload[MQTT_MAX_PACKET_SIZE];
  serializeJson(outbound_JSON_message, mqtt_payload, sizeof(mqtt_payload));
  this->mqtt.publish(this->mqtt_state_topic.c_str(), mqtt_payload, MQTT_RETAIN);
  Serial.print("[MQTT] State published: ");
  Serial.println(mqtt_payload);

}

void IotKernel::mqtt_publish_info(){
  
  JsonDocument outbound_JSON_message;
  
  outbound_JSON_message["type"] = this->device_type;
  outbound_JSON_message["nickname"] = this->config.nickname;
  outbound_JSON_message["version"] = this->firmware_version;
  outbound_JSON_message["address"] = WiFi.localIP().toString();
  
  char mqtt_payload[MQTT_MAX_PACKET_SIZE];
  serializeJson(outbound_JSON_message, mqtt_payload, sizeof(mqtt_payload));
  this->mqtt.publish(this->mqtt_info_topic.c_str(), mqtt_payload, MQTT_RETAIN);
  Serial.println("[MQTT] Info published");

}

void IotKernel::mqtt_publish_available(){
  this->mqtt.publish(this->mqtt_availability_topic.c_str(), "online", MQTT_RETAIN);
  Serial.println("[MQTT] Availability published");

}

void IotKernel::handle_mqtt(){
  if(WiFi.getMode() != WIFI_STA ) return;
  if(this->is_unset(this->config.mqtt.broker.host)) return;
  this->mqtt_connection_manager();
  this->mqtt.loop();
}
