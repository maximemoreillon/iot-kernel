#include "IotKernel.h"

const char* captivePortalPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>IoT Kernel captive portal</title>
</head>
<body>
  <p>Redirecting...</p>
  <script>setTimeout(() => window.location.replace('/'), 1000)</script>
</body>
</html>
)rawliteral";


String IotKernel::htmlProcessor(const String& var){

  if(var == "IOT_KERNEL_VERSION") return IOT_KERNEL_VERSION;

  else if(var == "DEVICE_NAME") return this->deviceName;
  else if(var == "DEVICE_TYPE") return this->deviceType;
  else if(var == "DEVICE_FIRMWARE_VERSION") return this->firmwareVersion;
  else if(var == "DEVICE_NICKNAME") return this->config.nickname;
  else if(var == "DEVICE_HOSTNAME") return this->get_hostname();

  else if(var == "MQTT_BROKER_HOST") return this->config.mqtt.broker.host;
  else if(var == "MQTT_BROKER_PORT") return String(this->config.mqtt.broker.port);
  else if(var == "MQTT_BROKER_SECURE") return this->config.mqtt.broker.secure;
  else if(var == "MQTT_USERNAME") return this->config.mqtt.username;
  // NOTE: passwords are never sent back to the browser
  else if(var == "MQTT_PASSWORD") return String();
  else if(var == "MQTT_STATUS") return this->get_mqtt_status();
  else if(var == "MQTT_BASE_TOPIC") return this->mqttBaseTopic;

  else if(var == "WIFI_MODE") return this->get_wifi_mode();
  else if(var == "WIFI_SSID") return this->config.wifi.ssid;
  else if(var == "WIFI_PASSWORD") return String();

  else if(var == "ADMIN_PASSWORD_STATUS") return this->has_admin_password() ? "set" : "not set";

  return String();

}

String IotKernel::get_mqtt_status(){
  if(this->is_unset(this->config.mqtt.broker.host)) return "disabled (no broker)";
  if(WiFi.getMode() != WIFI_STA) return "not connected (access point mode)";
  switch(this->mqtt.state()) {
    case MQTT_CONNECTED: return "connected";
    case MQTT_CONNECTION_TIMEOUT: return "connection timed out";
    case MQTT_CONNECTION_LOST: return "connection lost";
    case MQTT_CONNECT_FAILED: return "connection failed (broker unreachable)";
    case MQTT_DISCONNECTED: return "disconnected";
    case MQTT_CONNECT_BAD_PROTOCOL: return "rejected: bad protocol";
    case MQTT_CONNECT_BAD_CLIENT_ID: return "rejected: bad client ID";
    case MQTT_CONNECT_UNAVAILABLE: return "rejected: broker unavailable";
    case MQTT_CONNECT_BAD_CREDENTIALS: return "rejected: bad username or password";
    case MQTT_CONNECT_UNAUTHORIZED: return "rejected: not authorized";
    default: return String(this->mqtt.state());
  }
}

String IotKernel::htmlEscape(const String& input){
  // Template values are inserted in HTML text and attributes, e.g. value="%DEVICE_NICKNAME%"
  String output;
  output.reserve(input.length());
  for(unsigned int i = 0; i < input.length(); i++) {
    char c = input[i];
    switch(c) {
      case '&': output += "&amp;"; break;
      case '<': output += "&lt;"; break;
      case '>': output += "&gt;"; break;
      case '"': output += "&quot;"; break;
      case '\'': output += "&#39;"; break;
      default: output += c;
    }
  }
  return output;
}

boolean IotKernel::isSameOrigin(AsyncWebServerRequest *request){
  // Blocks cross-site requests (CSRF): a web page on another origin
  // submitting a form to the device sends its own Origin header.
  // Requests without an Origin header (e.g. curl) are allowed.
  if(!request->hasHeader("Origin")) return true;
  String origin = request->header("Origin");
  String host = request->host();
  if(origin == "http://" + host || origin == "https://" + host) return true;
  Serial.printf("[HTTP] Rejected cross-origin request from %s\n", origin.c_str());
  return false;
}

boolean IotKernel::isAuthorized(AsyncWebServerRequest *request){
  // Used in upload handlers: middlewares only run once the whole body has been received,
  // i.e. after the upload has been processed, so uploads must be checked on their first chunk
  if(!this->has_admin_password()) return true;
  if(this->auth_middleware.allowed(request)) return true;
  Serial.println("[HTTP] Rejected unauthenticated upload");
  return false;
}

String IotKernel::updatedPassword(AsyncWebServerRequest *request, const char* field, const char* clearField, const String& current){
  // Passwords are not sent to the browser, so an empty field means "keep the current one"
  if(request->hasArg(clearField)) return String();
  String password = request->arg(field);
  if(!password.length()) return current;
  return password;
}

String IotKernel::trimmedArg(AsyncWebServerRequest *request, const char* field){
  // Leading and trailing spaces are almost always typos (e.g. in the SSID)
  // NOTE: not used for passwords, in which spaces are valid
  String value = request->arg(field);
  value.trim();
  return value;
}

String IotKernel::sanitizeUploadFilename(const String& filename){
  // Keep only the file name (drop any path) and reject . and ..
  int slash = max(filename.lastIndexOf('/'), filename.lastIndexOf('\\'));
  String name = filename.substring(slash + 1);
  if(name.length() == 0 || name == "." || name == "..") return String();
  return name;
}


void IotKernel::http_setup(){
  Serial.println("[HTTP] Web server initialization");

  // Middlewares apply to every route, including the ones added by sketches with iot_kernel.http.on()
  this->http.addMiddleware(&this->same_origin_middleware);

  if(this->has_admin_password()) {
    this->auth_middleware.setUsername(WEB_SERVER_ADMIN_USERNAME);
    this->auth_middleware.setPassword(this->config.admin_password.c_str());
    this->auth_middleware.setAuthType(AsyncAuthType::AUTH_DIGEST);
    this->auth_middleware.generateHash();
    this->http.addMiddleware(&this->auth_middleware);
    Serial.println("[HTTP] Web server protected by the admin password");
  }

  // using lambda because otherwise method needs to be static
  // NOTE: values are HTML-escaped so that e.g. a " in the nickname does not break the form
  // NOTE: no-store because the ETag only depends on the file, not on the template values:
  // with caching, the browser would get "304 Not Modified" and show outdated values
  this->http.serveStatic("/", LittleFS, "/www", "no-store")
    .setDefaultFile("index.html")
    .setTemplateProcessor([this](const String& x) { return htmlEscape(htmlProcessor(x)); });

  this->http.on("/settings", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSettingsUpdate(request); });
  this->http.on("/update", HTTP_GET,[this](AsyncWebServerRequest *request) { handleFirmwareUpdateForm(request); });
  this->http.on("/update", HTTP_POST,
    [this](AsyncWebServerRequest *request) {
      handleFirmwareUpdateResult(request);
    },
    [this](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
      handleFirmwareUpdate(request, filename, index, data, len, final);
    }
  );
  this->http.on("/upload", HTTP_GET,[this](AsyncWebServerRequest *request) { handleUploadForm(request); });
  this->http.on("/upload", HTTP_POST,
    [](AsyncWebServerRequest*) {},
    [this](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
      handleUpload(request, filename, index, data, len, final);
    }
  );

  // NOTE: addHandler(new CaptiveRequestHandler()) did not work
  // NOTE: For iOS, redirects annot be used
  this->http.on("/generate_204", HTTP_ANY, [](AsyncWebServerRequest *req){ req->send(200, "text/html", captivePortalPage); });
  this->http.on("/gen_204", HTTP_ANY, [](AsyncWebServerRequest *req){ req->send(200, "text/html", captivePortalPage); });
  this->http.on("/hotspot-detect.html", HTTP_ANY, [](AsyncWebServerRequest *req){ req->send(200, "text/html", captivePortalPage); });
  this->http.on("/connecttest.txt", HTTP_ANY, [](AsyncWebServerRequest *req){ req->send(200, "text/html", captivePortalPage); });
  this->http.on("/redirect", HTTP_ANY, [](AsyncWebServerRequest *req){ req->send(200, "text/html", captivePortalPage); });
  this->http.on("/library/test/success.html", HTTP_ANY, [](AsyncWebServerRequest *req){ req->send(200, "text/html", captivePortalPage); });

  this->http.onNotFound([this](AsyncWebServerRequest *request) {
    Serial.printf("[HTTP] Not found: %s\n", request->url().c_str());
    handleNotFound(request);
  });
  
  this->http.begin();
}


void IotKernel::handleSettingsUpdate(AsyncWebServerRequest *request) {

  Serial.println("[HTTP] received settings update request");

  String wifi_password = this->updatedPassword(request, "wifi_password", "wifi_password_clear", this->config.wifi.password);
  String mqtt_password = this->updatedPassword(request, "mqtt_password", "mqtt_password_clear", this->config.mqtt.password);
  String admin_password = this->updatedPassword(request, "admin_password", "admin_password_clear", this->config.admin_password);

  JsonDocument doc;

  doc["nickname"] = this->trimmedArg(request, "nickname");
  doc["hostname"] = this->trimmedArg(request, "hostname");
  doc["admin_password"] = admin_password;

  JsonObject wifi  = doc["wifi"].to<JsonObject>();
  wifi["ssid"] = this->trimmedArg(request, "wifi_ssid");
  wifi["password"] = wifi_password;

  JsonObject mqtt  = doc["mqtt"].to<JsonObject>();
  mqtt["username"] = this->trimmedArg(request, "mqtt_username");
  mqtt["password"] = mqtt_password;

  JsonObject broker  = mqtt["broker"].to<JsonObject>();
  broker["host"] = this->trimmedArg(request, "mqtt_broker_host");
  broker["port"] = this->trimmedArg(request, "mqtt_broker_port");
  broker["secure"] = this->trimmedArg(request, "mqtt_broker_secure");

  Serial.println("[LittleFS] Writing config to config.json");

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("[LittleFS] Failed to open config file for writing");
    request->send(500, "text/plain", "Failed to write the configuration");
    return;
  }

  serializeJson(doc, configFile);
  Serial.println("[LittleFS] Finished writing config file");

  request->send(200, "text/html", "Rebooting...<script>setTimeout(() => window.location.replace('/'), 5000)</script>");

  rebootTimer.once(1, []() { ESP.restart(); });

}

void IotKernel::handleFirmwareUpdateForm(AsyncWebServerRequest *request){
  String html = ""
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='update'>"
      "<input type='submit' value='Upload'>"
    "</form>";

  request->send(200, "text/html", html);
}

#ifdef ESP32
void IotKernel::handleFirmwareUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {


  if (index == 0){

    if(!this->isSameOrigin(request) || !this->isAuthorized(request)) return;

    if(this->otaRequest){
      Serial.println("[Update] is already in progress");
      return;
    }


    Serial.printf("[Update] Updating ESP32 firmware using file %s\n", filename.c_str());

    this->otaRequest = request;

    if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
      Serial.println("[Update] begin failed");
      Update.printError(Serial);
    }
  }

  // Ignore chunks from any upload other than the one in progress
  if (request != this->otaRequest) return;

  if (!Update.hasError()) {
    if (Update.write(data, len) != len) {
      Serial.println("[Update] Write failed");
      Update.printError(Serial);
    }
    this->lastOtaWriteTime = millis();
  }

  if (final) {
    if (Update.end(true)){
      Serial.println("[Update] Update Successful");
    }
    else {
      Serial.println("[Update] end failed");
      Update.printError(Serial);
    }
  }

}
#else
void IotKernel::handleFirmwareUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {

  

  if (index == 0){

    if(!this->isSameOrigin(request) || !this->isAuthorized(request)) return;

    if(this->otaRequest){
      Serial.println("[Update] is already in progress");
      return;
    }

    Serial.printf("[Update] Updating ESP8266 firmware using file %s\n", filename.c_str());
    this->otaRequest = request;
    Update.runAsync(true);

    if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
      Serial.println("[Update] begin failed");
      Update.printError(Serial);
    }

  }

  // Ignore chunks from any upload other than the one in progress
  if (request != this->otaRequest) return;

  if (!Update.hasError()) {
    if(Update.write(data, len) != len){
      Serial.println("[Update] Write failed");
      Update.printError(Serial);
    }
    this->lastOtaWriteTime = millis();
  }

  if (final) {
    if (Update.end(true)){
      Serial.println("[Update] Update Successful");
    }
    else {
      Serial.println("[Update] end failed");
      Update.printError(Serial);
    }
  }
}
#endif

void IotKernel::handleFirmwareUpdateResult(AsyncWebServerRequest *request) {
  if (!this->otaRequest) {
    // e.g. the form was submitted without choosing a file
    request->send(400, "text/plain", "No firmware received");
    return;
  }
  if (request != this->otaRequest) {
    request->send(409, "text/plain", "Update already in progress");
    return;
  }
  this->otaRequest = nullptr;
  bool success = !Update.hasError();
  if (success) {
    request->send(200, "text/html", "Update successful. Rebooting...<script>setTimeout(() => window.location.replace('/'), 10000)</script>");
    rebootTimer.once(1, []() { ESP.restart(); });
  } else {
    // Stop the OTA timeout so loop() resumes normal operation
    this->lastOtaWriteTime = 0;
    request->send(500, "text/plain", "Update FAILED");
    Update.printError(Serial);
  }
}

void IotKernel::handleUploadForm(AsyncWebServerRequest *request){
  String html = ""
    "<form method='POST' action='/upload' enctype='multipart/form-data'>"
      "<input type='file' name='file'>"
      "<input type='submit' value='Upload'>"
    "</form>";

  request->send(200, "text/html", html);
}

void IotKernel::handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){

  // Uploading a file into the /www/ directory, which is served over HTTP by the web server

  if(!index){
    String name = this->sanitizeUploadFilename(filename);
    if(!this->isSameOrigin(request) || !this->isAuthorized(request)) {
      // File is not opened, so the rest of the upload is ignored
    }
    else if(!name.length()) {
      Serial.printf("[Upload] Rejected invalid file name: %s\n", filename.c_str());
    }
    else {
      Serial.printf("[Upload] Start: %s\n", name.c_str());
      request->_tempFile = LittleFS.open("/www/" + name, "w");
      if(!request->_tempFile) Serial.println("[Upload] Failed to open file for writing");
    }
  }

  if(len && request->_tempFile) {
    request->_tempFile.write(data,len);
  }

  if(final){
    if(!request->_tempFile) {
      request->send(400, "text/plain", "Upload rejected");
      return;
    }
    Serial.printf("[Upload] End: %s, %u B\n", filename.c_str(), index+len);
    request->_tempFile.close();
    request->redirect("/");
  }

}

void IotKernel::handleNotFound(AsyncWebServerRequest *request){
  request->send(404, "text/html", "Not found");
}
