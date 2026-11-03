#include "IotKernel.h"

static const char* wifi_status_description(wl_status_t status){
  switch(status) {
    case WL_NO_SSID_AVAIL: return "network not found";
    case WL_CONNECT_FAILED: return "connection failed";
#ifdef ESP8266
    case WL_WRONG_PASSWORD: return "wrong password";
#endif
    case WL_DISCONNECTED: return "timed out";
    default: return "unknown reason";
  }
}


boolean IotKernel::wifi_connected(){
  return WiFi.status() == WL_CONNECTED;
}

String IotKernel::get_hostname(){
  if(this->is_unset(this->config.hostname)) return this->deviceName;
  else return this->config.hostname;
}

String IotKernel::get_softap_ssid(){
  return this->deviceName;
}

String IotKernel::get_wifi_mode(){
  switch (WiFi.getMode()) {
    case WIFI_STA:
      return "STA";
    case WIFI_AP:
      return "AP";
    case WIFI_AP_STA:
      return "AP+STA (retrying the configured WiFi)";
    default:
      return "unknown";
  }
}


void IotKernel::wifi_set_hostname(){
  // NOTE: must be called before the station is enabled: ESP32 applies the hostname
  // when WiFi.mode() / WiFi.enableSTA() turns the station on, and ignores it afterwards
  String hostname = this->get_hostname();
  if(WiFi.setHostname(hostname.c_str())) {
    Serial.printf("[WiFi] Hostname: %s\n", hostname.c_str());
  }
  else {
    // ESP8266 rejects hostnames that are not made of letters, digits and -, or longer than 32 characters
    Serial.printf("[WiFi] Invalid hostname \"%s\" (letters, digits and - only, up to 32 characters), keeping the default\n", hostname.c_str());
  }
}

void IotKernel::wifi_begin(){
  // Starts connecting to the configured WiFi without waiting

  // NOTE: only the length of the password is printed, to help spot a mistyped or autofilled password
  Serial.printf("[WiFi] Attempting connection to \"%s\" (password: %u characters)\n", this->config.wifi.ssid.c_str(), this->config.wifi.password.length());

  // Use password or not depending of if provided
  if(!this->config.wifi.password.length()) WiFi.begin(this->config.wifi.ssid.c_str());
  else WiFi.begin(this->config.wifi.ssid.c_str(), this->config.wifi.password.c_str());
}

void IotKernel::attempt_sta(){

  WiFi.persistent(false);
  WiFi.disconnect();
  this->wifi_set_hostname();
  WiFi.mode(WIFI_STA);

  this->wifi_begin();

  // Attempt connection for a given amount of time
  unsigned long now = millis();
  while(millis() - now < WIFI_STA_CONNECTION_TIMEOUT && !wifi_connected()){
    delay(10); // Do nothing while connecting
  }

}

void IotKernel::start_ap(){

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(WIFI_AP_IP, WIFI_AP_IP, IPAddress(255, 255, 255, 0));

  // The admin password protects the access point too (WPA2 needs at least 8 characters)
  if(this->config.admin_password.length() >= WIFI_AP_MIN_PASSWORD_LENGTH) {
    WiFi.softAP(this->get_softap_ssid().c_str(), this->config.admin_password.c_str());
    Serial.printf("[WiFi] Access point initialized, SSID: %s (protected by the admin password)\n", this->get_softap_ssid().c_str());
  }
  else {
    if(this->has_admin_password()) {
      Serial.printf("[WiFi] Admin password shorter than %d characters, cannot be used for the access point\n", WIFI_AP_MIN_PASSWORD_LENGTH);
    }
    WiFi.softAP(this->get_softap_ssid().c_str());
    Serial.printf("[WiFi] Access point initialized, SSID: %s (open network)\n", this->get_softap_ssid().c_str());
  }

  this->wifi_ap_since = millis();
}

void IotKernel::wifi_setup() {

  Serial.println("[WiFi] Wifi setup");

  if(this->is_unset(this->config.wifi.ssid)) {
    Serial.println("[WiFi] No SSID configured, starting access point...");
    this->start_ap();
    return;
  }

  this->attempt_sta();

  if(!this->wifi_connected()){
    // If the connection to the wifi is not possible, create a Wifi access point
    wl_status_t status = WiFi.status();
    Serial.printf("[WiFi] Cannot connect to provided WiFi (status %d: %s), starting access point...\n", status, wifi_status_description(status));
    this->start_ap();
  }

}

void IotKernel::wifi_ap_retry_manager(){
  // When the device fell back to AP mode (e.g. the router was down at boot),
  // periodically try the configured WiFi again, without blocking and while keeping the AP up.

  if(this->wifi_sta_retry_start) {
    // A retry is in progress (AP+STA mode)

    if(this->wifi_connected()) {
      Serial.println("[WiFi] Connected to the configured WiFi, stopping access point");
      this->wifi_sta_retry_start = 0;
      this->dns_server.stop();
      WiFi.softAPdisconnect(true); // Leaves the device in STA mode
      this->mqtt_setup();
    }
    else if(millis() - this->wifi_sta_retry_start > WIFI_STA_CONNECTION_TIMEOUT) {
      wl_status_t status = WiFi.status();
      Serial.printf("[WiFi] Still cannot connect to the configured WiFi (status %d: %s)\n", status, wifi_status_description(status));
      this->wifi_sta_retry_start = 0;
      WiFi.disconnect();
      WiFi.enableSTA(false); // Back to AP only
      this->wifi_ap_since = millis();
    }
    return;
  }

  if(WiFi.getMode() != WIFI_AP) return;
  if(this->is_unset(this->config.wifi.ssid)) return;
  if(millis() - this->wifi_ap_since < WIFI_AP_STA_RETRY_PERIOD) return;

  // Do not disturb someone currently using the access point (e.g. configuring the device)
  if(WiFi.softAPgetStationNum() > 0) {
    this->wifi_ap_since = millis();
    return;
  }

  Serial.println("[WiFi] Retrying the configured WiFi");
  this->wifi_set_hostname();
  WiFi.enableSTA(true); // AP+STA
  this->wifi_begin();
  this->wifi_sta_retry_start = millis();
}

void IotKernel::wifi_connection_manager(){
  // Checks for changes in connection status

  if(this->wifi_connected() != this->wifi_last_connection_state) {
    this->wifi_last_connection_state = this->wifi_connected();

    if(this->wifi_connected()){
      Serial.print("[WIFI] Connected, IP: ");
      Serial.print(WiFi.localIP());
      Serial.print(", RSSI: ");
      Serial.println(WiFi.RSSI());
      if(this->wifi_connect_callback) this->wifi_connect_callback();
    }
    else {
      Serial.println("[WIFI] Disconnected");
      if(this->wifi_disconnect_callback) this->wifi_disconnect_callback();
    }
  }

}
