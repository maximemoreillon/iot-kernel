#include "IotKernel.h"

// Constructor
// Note how the web server is instantiated
IotKernel::IotKernel(String type, String version):
  same_origin_middleware([this](AsyncWebServerRequest *request, ArMiddlewareNext next) {
    // Blocks cross-site requests (CSRF) on every route that changes something
    if(request->method() != HTTP_GET && request->method() != HTTP_HEAD && !this->isSameOrigin(request)) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    next();
  }),
  http(WEB_SERVER_PORT){
  
  this->device_type = type;
  this->device_name = this->device_type + "-" + get_chip_id();
  this->firmware_version = version;

  this->otaRequest = nullptr;
  this->lastOtaWriteTime = 0;

  this->wifi_last_connection_state = false;
  this->mqtt_last_connection_state = false;
  this->mqtt_last_connection_attempt = 0;
  this->wifi_ap_since = 0;
  this->wifi_sta_retry_start = 0;
}

void IotKernel::init(){

  if(!Serial) Serial.begin(115200);

  Serial.printf("\n\n[IoT Kernel] v%s initializing...\n",IOT_KERNEL_VERSION);

  this->wifi_client_secure.setInsecure();

  this->spiffs_setup();
  this->get_config_from_spiffs();
  this->wifi_setup();

  if(WiFi.getMode() == WIFI_STA) {
    this->mqtt_setup();
  }

  else if(WiFi.getMode() == WIFI_AP) {
    Serial.println("[DNS] Starting DNS server");
    this->dns_server.start(DNS_PORT, "*", WIFI_AP_IP);
  }

  this->http_setup();

  Serial.println("[IoT Kernel] init complete");
}

void IotKernel::loop(){

  if(this->lastOtaWriteTime){

    // Needs to be cast a long somehow
    long diff = millis() - this->lastOtaWriteTime;
    if (diff > 10000) {
      Serial.printf("[Update] Update timed out, resetting...\n");
      ESP.restart();
    }
  } else {
    
    this->wifi_connection_manager();
    this->wifi_ap_retry_manager();

    if(WiFi.getMode() == WIFI_STA) {
      this->handle_mqtt();
    }

    else {
      // AP mode, or AP+STA while retrying the configured WiFi
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

boolean IotKernel::has_admin_password() {
  return !this->is_unset(this->config.admin_password);
}

void IotKernel::onWifiConnect(std::function<void()> callback) {
  this->wifi_connect_callback = callback;
}

void IotKernel::onWifiDisconnect(std::function<void()> callback) {
  this->wifi_disconnect_callback = callback;
}

void IotKernel::onMqttConnect(std::function<void()> callback) {
  this->mqtt_connect_callback = callback;
}

void IotKernel::onMqttDisconnect(std::function<void()> callback) {
  this->mqtt_disconnect_callback = callback;
}



