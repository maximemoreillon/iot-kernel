#include "IotKernel.h"

class CaptiveRequestHandler : public AsyncWebHandler {
  // Captive portal
  // Maybe not the ideal place to have this

  public:

    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}

    bool canHandle(AsyncWebServerRequest *request){
      return true;
    }

    void handleRequest(AsyncWebServerRequest *request) {
      request->redirect("/");
    }
};

String IotKernel::htmlProcessor(const String& var){

  if(var == "IOT_KERNEL_VERSION") return IOT_KERNEL_VERSION;

  else if(var == "DEVICE_NAME") return this->device_name;
  else if(var == "DEVICE_TYPE") return this->device_type;
  else if(var == "DEVICE_FIRMWARE_VERSION") return this->firmware_version;
  else if(var == "DEVICE_NICKNAME") return this->config.nickname;
  else if(var == "DEVICE_STATE") return this->device_state;

  else if(var == "MQTT_BROKER_HOST") return this->config.mqtt.broker.host;
  else if(var == "MQTT_BROKER_PORT") return String(this->config.mqtt.broker.port);
  else if(var == "MQTT_BROKER_SECURE") return this->config.mqtt.broker.secure;
  else if(var == "MQTT_USERNAME") return this->config.mqtt.username;
  else if(var == "MQTT_PASSWORD") return this->config.mqtt.password;
  else if(var == "MQTT_STATUS") return this->mqtt.connected() ? "connected" : "disconnected";
  else if(var == "MQTT_STATUS_TOPIC") return this->mqtt_status_topic;
  else if(var == "MQTT_COMMAND_TOPIC") return this->mqtt_command_topic;

  else if(var == "WIFI_MODE") return this->get_wifi_mode();
  else if(var == "WIFI_SSID") return this->config.wifi.ssid;
  else if(var == "WIFI_PASSWORD") return this->config.wifi.password;
  else if(var == "WIFI_DATALIST_OPTIONS") return this->format_wifi_datalist_options();

  return String();

}


void IotKernel::http_setup(){
  Serial.println("[HTTP] Web server initialization");

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  // using lamba because otherwise method needs to be static
  this->http.serveStatic("/", LittleFS, "/www")
    .setDefaultFile("index.html")
    .setTemplateProcessor([this](const String& x) { return htmlProcessor(x); });
    //.setTemplateProcessor([this](auto x) { return htmlProcessor(x); });
    //.setTemplateProcessor(htmlProcessor);

  this->http.on("/settings", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSettingsUpdate(request); });

  this->http.on("/update", HTTP_GET,[this](AsyncWebServerRequest *request) { handleFirmwareUpdateForm(request); });
  this->http.on("/update", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
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

  this->http.onNotFound([this](AsyncWebServerRequest *request) { handleNotFound(request); });
  this->http.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
  this->http.begin();
}


void IotKernel::handleSettingsUpdate(AsyncWebServerRequest *request) {

  StaticJsonDocument<1024> doc;

  doc["nickname"] = request->arg("nickname");

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

  Serial.println(request->arg("mqtt_broker_secure"));

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("[LittleFS] Failed to open config file for writing");
    request->send(500, "text/html", "Failed to acess SPIFFS");
    return;
  }

  serializeJson(doc, configFile);
  Serial.println("[SPIFFS] Finished writing config file");

  String reboot_html = "Rebooting...<script>setTimeout(() => window.location.replace('/'), 5000)</script>";

  request->send(200, "text/html", reboot_html);

  this->delayed_reboot();

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

  if (!index){
    Serial.println("[Update] Firmware update started");
    //size_t content_len = request->contentLength();
    // if filename includes spiffs, update the spiffs partition
    // int cmd = (filename.indexOf("spiffs") > -1) ? U_PART : U_FLASH;
    //
    // if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
    //   Update.printError(Serial);
    // }
    if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  }

  if (final) {
    if (!Update.end(true)){
      Update.printError(Serial);
      request->send(500, "text/html", "Firmware update failed");
    }
    else {
      Serial.println("[Update] Update complete");
      String reboot_html = "Rebooting...<script>setTimeout(() => window.location.replace('/'), 5000)</script>";
      request->send(200, "text/html", reboot_html);
      this->delayed_reboot();
    }
  }

}
#else
void IotKernel::handleFirmwareUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {

  if (!index){

    Serial.print("[Update] Updating firmware using file ");
    Serial.println(filename);
    Serial.print("[Update] Size: ");
    Serial.print(request->contentLength());
    Serial.print(", available: ");
    Serial.print(ESP.getFreeSketchSpace());
    Serial.println();

    Update.runAsync(true);

    if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
      Update.printError(Serial);
    }
  }

  if(!Update.hasError()){
    if(Update.write(data, len) != len){
      Update.printError(Serial);
    }
  }


  if (final) {
    if (!Update.end(true)){
      Update.printError(Serial);
      request->send(500, "text/html", "Firmware update failed");
    }
    else {
      Serial.println("[Update] Update complete");
      String reboot_html = "Rebooting...<script>setTimeout(() => window.location.replace('/'), 5000)</script>";
      request->send(200, "text/html", reboot_html);
      this->delayed_reboot();
    }
  }
}
#endif


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
