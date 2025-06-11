#include "IotKernel.h"



boolean IotKernel::wifi_connected(){
  return WiFi.status() == WL_CONNECTED;
}

String IotKernel::get_hostname(){
  if(this->is_unset(this->config.hostname)) return this->device_name;
  else return this->config.hostname;
}

String IotKernel::get_softap_ssid(){
  return this->device_name;
}

String IotKernel::get_wifi_mode(){
  switch (WiFi.getMode()) {
    case 1:
      return "STA";
    case 2:
      return "AP";
    default:
      return "unknown";
  }
}

void IotKernel::scan_wifi(){
  Serial.println("[Wifi] Scan start... ");
  this->found_wifi_count = WiFi.scanNetworks();
  Serial.printf("[Wifi] Scan result: %u network(s) found\n", this->found_wifi_count);
}

String IotKernel::format_wifi_datalist_options(){
  String wifi_datalist_options = "";
  for (int i = 0; i < found_wifi_count; i++) {
    wifi_datalist_options = wifi_datalist_options + "<option value=\"" + WiFi.SSID(i) + "\">";
  }
  return wifi_datalist_options;
}


void IotKernel::attempt_sta(){

  WiFi.persistent(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);

  Serial.printf("[WiFi] Setting hostname to %s\n", this->get_hostname().c_str());

  WiFi.setHostname(this->get_hostname().c_str()); // ESP32
  WiFi.hostname(this->get_hostname().c_str()); // ESP8266

  // This takes time so a bit annoying
  this->scan_wifi();

  Serial.printf("[WiFi] Attempting connection to %s\n", this->config.wifi.ssid.c_str());

  // Use password or not depending of if provided
  if(!this->config.wifi.password.length()) WiFi.begin(this->config.wifi.ssid.c_str());
  else WiFi.begin(this->config.wifi.ssid.c_str(), this->config.wifi.password.c_str());

  // Attempt connection for a given amount of time
  long now = millis();
  while(millis() - now < WIFI_STA_CONNECTION_TIMEOUT && !wifi_connected()){
    delay(10); // Do nothing while connecting
  }

}

void IotKernel::wifi_setup() {

  Serial.println("[WiFi] Wifi setup");
  this->attempt_sta();

  if(this->wifi_connected()){
    // Serial.print("[WIFI] Connected, IP: ");
    // Serial.println(WiFi.localIP());
  }

  else {
    /*
     * If the connection to the wifi is not possible,
     * Create a Wifi access point
     * Create a web server
     * Direct clients to the web server upon connection to the Wifi Access point
     */

    Serial.println("[Wifi] Cannot connect to provided WiFi, starting access point...");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(WIFI_AP_IP, WIFI_AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(this->get_softap_ssid().c_str());

    Serial.printf("[WiFi] Access point initialized, SSID: %s\n", this->get_softap_ssid().c_str());
  }

}

void IotKernel::wifi_connection_manager(){
  // Checks for changes in connection status
  // Currently for STA mode
  // TODO: consider removing as no practical use

  static boolean last_connection_state = false;

  if(this->wifi_connected() != last_connection_state) {
    last_connection_state = this->wifi_connected();


    if(this->wifi_connected()){
      Serial.print("[WIFI] Connected, IP: ");
      Serial.print(WiFi.localIP());
      Serial.print(", RSSI: ");
      Serial.println(WiFi.RSSI());
    }
    else {
      Serial.println("[WIFI] Disconnected");
    }
  }

}
