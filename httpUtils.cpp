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

  else if(var == "DEVICE_NAME") return this->device_name;
  else if(var == "DEVICE_TYPE") return this->device_type;
  else if(var == "DEVICE_FIRMWARE_VERSION") return this->firmware_version;
  else if(var == "DEVICE_NICKNAME") return this->config.nickname;
  else if(var == "DEVICE_HOSTNAME") return this->get_hostname();
  else if(var == "DEVICE_STATE") return this->device_state;

  else if(var == "MQTT_BROKER_HOST") return this->config.mqtt.broker.host;
  else if(var == "MQTT_BROKER_PORT") return String(this->config.mqtt.broker.port);
  else if(var == "MQTT_BROKER_SECURE") return this->config.mqtt.broker.secure;
  else if(var == "MQTT_USERNAME") return this->config.mqtt.username;
  else if(var == "MQTT_PASSWORD") return this->config.mqtt.password;
  else if(var == "MQTT_STATUS") return String(this->mqtt.state());
  else if(var == "MQTT_STATE_TOPIC") return this->mqtt_state_topic;
  else if(var == "MQTT_COMMAND_TOPIC") return this->mqtt_command_topic;

  else if(var == "WIFI_MODE") return this->get_wifi_mode();
  else if(var == "WIFI_SSID") return this->config.wifi.ssid;
  else if(var == "WIFI_PASSWORD") return this->config.wifi.password;

  return String();

}


void IotKernel::http_setup(){
  Serial.println("[HTTP] Web server initialization");

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  // using lambda because otherwise method needs to be static
  this->http.serveStatic("/", LittleFS, "/www")
    .setDefaultFile("index.html")
    .setTemplateProcessor([this](const String& x) { return htmlProcessor(x); });

  // TODO: add a GET /settings endpoint that returns JSON

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
    [](AsyncWebServerRequest *request) {},
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

  StaticJsonDocument<1024> doc;

  doc["nickname"] = request->arg("nickname");
  doc["hostname"] = request->arg("hostname");

  JsonObject wifi  = doc.createNestedObject("wifi");
  wifi["ssid"] = request->arg("wifi_ssid");
  wifi["password"] = request->arg("wifi_password");

  JsonObject mqtt  = doc.createNestedObject("mqtt");
  mqtt["username"] = request->arg("mqtt_username");
  mqtt["password"] = request->arg("mqtt_password");

  JsonObject broker  = mqtt.createNestedObject("broker");
  broker["host"] = request->arg("mqtt_broker_host");
  broker["port"] = request->arg("mqtt_broker_port");
  broker["secure"] = request->arg("mqtt_broker_secure");

  Serial.println("[LittleFS] Writing config to config.json");

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("[LittleFS] Failed to open config file for writing");
    request->send(500, "text/html", "Failed to acess SPIFFS");
    return;
  }

  serializeJson(doc, configFile);
  Serial.println("[SPIFFS] Finished writing config file");

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

    if(this->otaInProgress == true){
      Serial.println("[Update] is already in progress");
      return;
    }

    Serial.printf("[Update] Updating firmware using file %s\n", filename.c_str());
    this->otaInProgress = true;

    if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
      Serial.println("[Update] begin failed");
      Update.printError(Serial);
    }
  }
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

    if(this->otaInProgress == true){
      Serial.println("[Update] is already in progress");
      return;
    }

    Serial.printf("[Update] Updating firmware using file %s\n", filename.c_str());
    this->otaInProgress = true;
    Update.runAsync(true);

    if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
      Serial.println("[Update] begin failed");
      Update.printError(Serial);
    } 

  }

  if (!Update.hasError()) {
    if(Update.write(data, len) != len){
      Serial.println("[Update] Write failed");
      Update.printError(Serial);
    }
    // TODO: Here might notbe the best place for this
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
  this->otaInProgress = false;
  bool success = !Update.hasError();
  if (success) {
    request->send(200, "text/html", "Update successful. Rebooting...<script>setTimeout(() => window.location.replace('/'), 10000)</script>");
    rebootTimer.once(1, []() { ESP.restart(); });
  } else {
    request->send(500, "text/plain", "Update FAILED");
    Update.printError(Serial);
  }
}

void IotKernel::handleUploadForm(AsyncWebServerRequest *request){
  String html = ""
    "<form method='POST' action='/upload' enctype='multipart/form-data'>"
      "<input type='file' name='update'>"
      "<input type='submit' value='Upload'>"
    "</form>";

  request->send(200, "text/html", html);
}

void IotKernel::handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){

  // Uploading a file into the /www/ directory, which is served over HTTP by the web server

  if(!index){
    Serial.printf("[Upload] Start: %s\n", filename.c_str());
    const String path = "/www/" + filename;
    request->_tempFile = LittleFS.open(path, "w");
  }

  if(len) {
    request->_tempFile.write(data,len);
  }

  if(final){
    Serial.printf("[Upload] End: %s, %u B\n", filename.c_str(), index+len);
    request->_tempFile.close();
    request->redirect("/");
  }

}

void IotKernel::handleNotFound(AsyncWebServerRequest *request){
  request->send(404, "text/html", "Not found");
}
