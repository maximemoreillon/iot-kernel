#include "IotKernel.h"



boolean IotKernel::wifi_connected(){
  return WiFi.status() == WL_CONNECTED;
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
  Serial.println("[Wifi] Scan start ... ");

  this->found_wifi_count = WiFi.scanNetworks();

  Serial.print("[Wifi] scan result: ");
  Serial.print(this->found_wifi_count);
  Serial.println(" network(s) found");
}

String IotKernel::format_wifi_datalist_options(){
  String wifi_datalist_options = "";
  for (int i = 0; i < found_wifi_count; i++)
  {
    wifi_datalist_options = wifi_datalist_options + "<option value=\"" + WiFi.SSID(i) + "\">";
  }
  return wifi_datalist_options;
}


void IotKernel::attempt_sta(){

  WiFi.persistent(false);

  WiFi.disconnect();
  

  WiFi.mode(WIFI_STA);
  
  // Hostname not working properly
  WiFi.setHostname(this->device_name.c_str()); // ESP32
  WiFi.hostname(this->device_name.c_str()); // ESP8266


  this->scan_wifi();

  String wifi_sta_ssid = this->config.wifi.ssid;
  String wifi_sta_password = this->config.wifi.password;

  Serial.print("[WiFi] Attempting connection to ");
  Serial.print(wifi_sta_ssid);
  Serial.println("");

  // Use password or not depending of if provided
  if(wifi_sta_password == "") WiFi.begin(wifi_sta_ssid.c_str());
  else WiFi.begin(wifi_sta_ssid.c_str(), wifi_sta_password.c_str());

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
    Serial.print("[WIFI] Connected, IP: ");
    Serial.println(WiFi.localIP());
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

    // Debugging
    Serial.println("[WiFi] Access point initialized");
  }

}

void IotKernel::wifi_connection_manager(){
  // Checks for changes in connection status
  // Currently for STA mode

  static boolean last_connection_state = false;

  if(this->wifi_connected() != last_connection_state) {
    last_connection_state = this->wifi_connected();


    if(this->wifi_connected()){
      Serial.print("[WIFI] Connected, IP: ");
      Serial.println(WiFi.localIP());
    }
    else {
      Serial.println("[WIFI] Disconnected");
    }
  }

}
