#include "IotKernel.h"

void IotKernel::spiffs_setup() {

  if (!LittleFS.begin()) {
    Serial.println("[SPIFFS] Failed to mount file system");
  }
  else {
    Serial.println("[SPIFFS] File system mounted");
  }
}

void IotKernel::get_config_from_spiffs(){

  Serial.println("[SPIFFS] Loading config from config.json...");

  JsonDocument doc;


  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("[SPIFFS] Failed to open config file");
    return;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("[SPIFFS] Config file size is too large");
    return;
  }

  auto error = deserializeJson(doc, configFile);
  configFile.close();
  if (error) {
    Serial.println("[SPIFFS] Failed to parse config file");
    return;
  }

  // load file content into custom config structure
  // Missing keys default to an empty string (not "null")
  this->config.nickname = doc["nickname"] | "";
  this->config.hostname = doc["hostname"] | "";
  this->config.wifi.ssid = doc["wifi"]["ssid"] | "";
  this->config.wifi.password = doc["wifi"]["password"] | "";
  this->config.mqtt.broker.host = doc["mqtt"]["broker"]["host"] | "";
  // NOTE: as<int>() also parses the port when stored as a string by the web form
  this->config.mqtt.broker.port = doc["mqtt"]["broker"]["port"].as<int>();
  this->config.mqtt.broker.secure = doc["mqtt"]["broker"]["secure"] | "";
  this->config.mqtt.username = doc["mqtt"]["username"] | "";
  this->config.mqtt.password = doc["mqtt"]["password"] | "";

  Serial.println("[SPIFFS] Successfully loaded config from config.json");

}
