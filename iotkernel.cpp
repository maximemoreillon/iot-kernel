#include "iotKernel.h"

IotKernel::IotKernel(String type, String version): web_server(WEB_SERVER_PORT){
  // Constructor
  // Note how the web server is instantiated

  

  this->device_type = type;
  this->device_name = this->device_type + "-" + String(ESP.getChipId(), HEX);
  this->firmware_version = version;

  this->device_state = "off";
}

void IotKernel::init(){

  if(!Serial){
    Serial.begin(115200);
  }

  Serial.println("IoT Kernel init");

  this->wifi_client_secure.setInsecure();

  this->spiffs_setup();
  this->get_config_from_spiffs();

  this->wifi_setup();
  this->MQTT_setup();
  this->dns_server.start(DNS_PORT, "*", WIFI_AP_IP);
  this->web_server_setup();
}

void IotKernel::loop(){
  this->wifi_connection_manager();
  this->MQTT_connection_manager();
  this->MQTT_client.loop();
  this->dns_server.processNextRequest();
  this->handle_reboot();
}



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
