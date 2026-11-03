#include "IotKernel.h"

// Constructor
// Note how the web server is instantiated
IotKernel::IotKernel(String type, String version): http(WEB_SERVER_PORT){
  
  this->device_type = type;
  this->device_name = this->device_type + "-" + get_chip_id();
  this->firmware_version = version;

  // this->device_state = "off";

  this->otaRequest = nullptr;
  this->lastOtaWriteTime = 0;

  this->wifi_last_connection_state = false;
  this->mqtt_last_connection_state = false;
  this->mqtt_last_connection_attempt = 0;
  this->wifi_ap_since = 0;
  this->wifi_sta_retry_start = 0;
}

void IotKernel::init(String state){

  this->device_state = state;

  if(!Serial) Serial.begin(115200);

  Serial.printf("\n\n[IoT Kernel] v%s initializing...\n",IOT_KERNEL_VERSION);

  this->wifi_client_secure.setInsecure();

  // Set once here rather than in mqtt_setup(), which can run again later (WiFi recovery from AP mode):
  // sketches that replace it with iot_kernel.mqtt.setCallback() after init() must keep their callback
  this->mqtt.setCallback([this](char* topic, byte* payload, unsigned int payload_length) { mqtt_message_callback(topic, payload, payload_length); });

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

void IotKernel::onStateChange(std::function<void(const String&)> callback) {
  this->state_change_callback = callback;
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



