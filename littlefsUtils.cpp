#include "IotKernel.h"

void IotKernel::littlefs_setup() {

  if (!LittleFS.begin()) {
    Serial.println("[LittleFS] Failed to mount file system");
  }
  else {
    Serial.println("[LittleFS] File system mounted");
  }
}

void IotKernel::load_config(){

  Serial.println("[LittleFS] Loading config from config.json...");

  JsonDocument doc;


  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("[LittleFS] Failed to open config file");
    return;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("[LittleFS] Config file size is too large");
    return;
  }

  auto error = deserializeJson(doc, configFile);
  configFile.close();
  if (error) {
    Serial.println("[LittleFS] Failed to parse config file");
    return;
  }

  // load file content into custom config structure
  // Missing keys default to an empty string (not "null")
  this->config.nickname = doc["nickname"] | "";
  this->config.hostname = doc["hostname"] | "";
  this->config.admin_password = doc["admin_password"] | "";
  this->config.wifi.ssid = doc["wifi"]["ssid"] | "";
  this->config.wifi.password = doc["wifi"]["password"] | "";
  this->config.mqtt.broker.host = doc["mqtt"]["broker"]["host"] | "";
  // NOTE: as<int>() also parses the port when stored as a string by the web form
  // and returns 0 when the port is missing or empty
  this->config.mqtt.broker.port = doc["mqtt"]["broker"]["port"].as<int>();
  if(!this->config.mqtt.broker.port) this->config.mqtt.broker.port = MQTT_DEFAULT_PORT;
  this->config.mqtt.broker.secure = doc["mqtt"]["broker"]["secure"] | "";
  this->config.mqtt.username = doc["mqtt"]["username"] | "";
  this->config.mqtt.password = doc["mqtt"]["password"] | "";

  Serial.println("[LittleFS] Successfully loaded config from config.json");

}
