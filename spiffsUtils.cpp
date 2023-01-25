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

  StaticJsonDocument<1024> doc;


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

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);


  auto error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.println("[SPIFFS] Failed to parse config file");
    return;
  }

  // export file content to custom config structure
  this->config.nickname = doc["nickname"].as<String>();
  this->config.hostname = doc["hostname"].as<String>();
  this->config.wifi.ssid = doc["wifi"]["ssid"].as<String>();
  this->config.wifi.password = doc["wifi"]["password"].as<String>();
  this->config.mqtt.broker.host = doc["mqtt"]["broker"]["host"].as<String>();
  this->config.mqtt.broker.port = doc["mqtt"]["broker"]["port"].as<int>();
  this->config.mqtt.broker.secure = doc["mqtt"]["broker"]["secure"].as<String>();
  this->config.mqtt.username = doc["mqtt"]["username"].as<String>();
  this->config.mqtt.password = doc["mqtt"]["password"].as<String>();

}
