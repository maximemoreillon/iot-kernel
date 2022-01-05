#include "IotKernel.h"

IotKernel::IotKernel(String type, String version): http(WEB_SERVER_PORT){
  // Constructor
  // Note how the web server is instantiated

  this->device_type = type;
  this->device_name = this->device_type + "-" + get_chip_id();
  this->firmware_version = version;

  this->device_state = "off";
}

void IotKernel::init(){

  if(!Serial){
    Serial.begin(115200);
  }

  Serial.println("[IoT Kernel] init...");

  this->wifi_client_secure.setInsecure();

  this->spiffs_setup();
  this->get_config_from_spiffs();
  this->wifi_setup();
  this->dns_server.start(DNS_PORT, "*", WIFI_AP_IP);
  this->mqtt_setup();
  this->http_setup();

  Serial.println("[IoT Kernel] init complete");
}

void IotKernel::loop(){
  this->wifi_connection_manager();
  this->mqtt_connection_manager();
  this->mqtt.loop();
  this->dns_server.processNextRequest();
  this->handle_reboot();
}

#ifdef ESP32
String IotKernel::get_chip_id(){
  uint32_t chipId = 0;

  for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }

  return String(chipId, HEX);
}
#else
String IotKernel::get_chip_id(){
  return String(ESP.getChipId(), HEX);
}
#endif

void IotKernel::delayed_reboot(){
  this->reboot_pending = true;
}

void IotKernel::handle_reboot(){
  static boolean reboot_request_acknowledged = false;
  static long reboot_request_time = 0;

  // Save the reboot request time
  if(reboot_pending != reboot_request_acknowledged && this->reboot_pending){
    Serial.println("[Reboot] Reboot request acknowledged");
    reboot_request_acknowledged = true;
    reboot_request_time = millis();
  }

  if(reboot_request_acknowledged && millis() - reboot_request_time > 1000){
    Serial.println("[Reboot] Rebooting now");
    ESP.restart();
  }
}
