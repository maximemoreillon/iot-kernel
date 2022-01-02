#include "iotKernel.h"

class CaptiveRequestHandler : public AsyncWebHandler {
  // Captive portal
  // Not the best place to have this

  public:

    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}

    bool canHandle(AsyncWebServerRequest *request){
      //request->addInterestingHeader("ANY");
      return true;
    }

    void handleRequest(AsyncWebServerRequest *request) {
      request->redirect("/");
    }
};

String IotKernel::htmlProcessor(const String& var){

  if(var == "IOT_KERNEL_VERSION") return IOT_KERNEL_VERSION;
  else if(var == "DEVICE_NAME") return this->get_device_name();
  else if(var == "DEVICE_TYPE") return this->device_type;
  else if(var == "DEVICE_FIRMWARE_VERSION") return this->firmware_version;
  else if(var == "DEVICE_NICKNAME") return this->config.nickname;
  else if(var == "DEVICE_STATE") return this->device_state;

  else if(var == "MQTT_BROKER_HOST") return this->config.mqtt.broker.host;
  else if(var == "MQTT_BROKER_PORT") return String(this->config.mqtt.broker.port);
  else if(var == "MQTT_USERNAME") return this->config.mqtt.username;
  else if(var == "MQTT_PASSWORD") return this->config.mqtt.password;
  else if(var == "MQTT_STATUS") return this->MQTT_client.connected() ? "connected" : "disconnected";

  else if(var == "WIFI_MODE") return this->get_wifi_mode();
  else if(var == "WIFI_SSID") return this->config.wifi.ssid;
  else if(var == "WIFI_PASSWORD") return this->config.wifi.password;
  else if(var == "WIFI_DATALIST_OPTIONS") return this->format_wifi_datalist_options();

  return String();

}


void IotKernel::web_server_setup(){
  Serial.println("[Web server] Web server initialization");

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  // using lamba because otherwise method needs to be static
  this->web_server.serveStatic("/", LittleFS, "/www")
    .setDefaultFile("index.html")
    .setTemplateProcessor([this](auto x) { return htmlProcessor(x); });

  this->web_server.on("/settings", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSettingsUpdate(request); });

  this->web_server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    [this](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
      handleFirmwareUpdate(request, filename, index, data, len, final);
    }
  );


  this->web_server.on("/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    [this](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
      handleUiUpload(request, filename, index, data, len, final);
    }
  );


  this->web_server.onNotFound([this](AsyncWebServerRequest *request) { handleNotFound(request); });

  this->web_server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);


  this->web_server.begin();
}


void IotKernel::handleNotFound(AsyncWebServerRequest *request){
  request->send(404, "text/html", "Not found");
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

  // Reboot
  this->delayed_reboot();

}

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

void IotKernel::handleUiUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){


  if(!index){
    Serial.printf("[UI upload] UploadStart: %s\n", filename.c_str());
    const String path = "/www/" + filename;
    request->_tempFile = LittleFS.open(path, "w");
  }

  if(len) {
    request->_tempFile.write(data,len);
  }


  if(final){
    Serial.printf("[UI upload] UploadEnd: %s, %u B\n", filename.c_str(), index+len);
    request->_tempFile.close();
    request->redirect("/");
  }


}
