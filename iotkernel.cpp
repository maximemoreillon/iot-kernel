#include "iotKernel.h"

IotKernel::IotKernel(String type, String version): web_server(WEB_SERVER_PORT){
  // Constructor

  this->MQTT_client.setClient(this->wifi_client);

  this->device_type = type;
  this->firmware_version = version;

  this->device_state = "off";
}

void IotKernel::init(){

  if(!Serial){
    Serial.begin(115200);
  }

  Serial.println("IoT Kernel init");

  this->spiffs_setup();
  this->get_config_from_spiffs();
  this->wifi_client.setInsecure();
  this->wifi_setup();
  this->MQTT_setup();
  this->dns_server.start(DNS_PORT, "*", WIFI_AP_IP);
  this->web_server_setup();

  MDNS.begin(get_device_name().c_str());
}

void IotKernel::loop(){
  this->wifi_connection_manager();
  this->MQTT_connection_manager();
  this->MQTT_client.loop();
  this->dns_server.processNextRequest();
  this->handle_reboot();

  MDNS.update();
}

String IotKernel::get_device_name(){
  return this->device_type + "-" + String(ESP.getChipId(), HEX);
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
