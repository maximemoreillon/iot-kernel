#include "IotKernel.h"

// Constructor
// Note how the web server is instantiated
IotKernel::IotKernel(String type, String version): http(WEB_SERVER_PORT){
  
  this->device_type = type;
  this->device_name = this->device_type + "-" + get_chip_id();
  this->firmware_version = version;

  this->device_state = "off";

  this->otaInProgress = false;
  this->lastOtaWriteTime = 0;
}

void IotKernel::init(){

  if(!Serial) Serial.begin(115200);

  Serial.printf("\n\n[IoT Kernel] v%s initializing...\n",IOT_KERNEL_VERSION);

  this->wifi_client_secure.setInsecure();

  this->spiffs_setup();
  this->get_config_from_spiffs();
  this->wifi_setup();

  if(WiFi.getMode() == WIFI_MODE_STA) {
    this->mqtt_setup();
  }

  else if(WiFi.getMode() == WIFI_MODE_AP) {
    this->dns_server.start(DNS_PORT, "*", WIFI_AP_IP);
  }

  this->http_setup();

  Serial.println("[IoT Kernel] init complete");
}

void IotKernel::loop(){

  if(this->otaInProgress){
    if (millis() - this->lastOtaWriteTime > 10000) {
      Serial.println("[Update] Update timed out, resetting...");
      ESP.restart();
    }
  } else {
    
    // Not really important, just used for Serial
    this->wifi_connection_manager();
    
    if(WiFi.getMode() == WIFI_MODE_STA) {
      this->handle_mqtt();
    }
    
    else if(WiFi.getMode() == WIFI_MODE_AP) {
      this->dns_server.processNextRequest();
    }
  } 
  
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

boolean IotKernel::is_unset(String input) {
  return !input.length() || input == "null";
}



