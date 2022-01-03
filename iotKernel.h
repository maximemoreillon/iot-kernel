#ifndef IotKernel_h
#define IotKernel_h

#include <ESP8266WiFi.h> // Main ESP8266 library
#include <PubSubClient.h> // MQTT
#include <WiFiClientSecure.h> // Wifi client, used by MQTT
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h> // DNS server to redirect wifi clients to the web server
#include <ArduinoJson.h> // JSON, used for the formatting of messages sent to the server
#include <Updater.h>
//#include <ESP8266mDNS.h>
#include <LittleFS.h>


// Wifi
#define WIFI_STA_CONNECTION_TIMEOUT 20000
#define WIFI_AP_IP IPAddress(192, 168, 4, 1)

// MQTT
#define MQTT_RECONNECT_PERIOD 5000
#define MQTT_QOS 1
#define MQTT_RETAIN true

// MISC
#define DNS_PORT 53
#define WEB_SERVER_PORT 80

#define IOT_KERNEL_VERSION "0.1.3"

struct WifiConfig {
  String ssid;
  String password;
};

struct MqttBrokerConfig {
  String host;
  int port;
  String secure;
};

struct MqttConfig {
  MqttBrokerConfig broker;
  String username;
  String password;
};

struct DeviceConfig {
  String nickname;
  WifiConfig wifi;
  MqttConfig mqtt;
};

class IotKernel {

  private:

    WiFiClient wifi_client;
    WiFiClientSecure wifi_client_secure;
    DNSServer dns_server;
    DeviceConfig config;

    int found_wifi_count;
    boolean reboot_pending;
    String device_type;
    String device_name;
    String firmware_version;


    // Misc
    void handle_reboot();
    void delayed_reboot();
    String get_device_name();

    // SPIFFS
    void spiffs_setup();
    void get_config_from_spiffs();

    // Wifi
    boolean wifi_connected();
    String get_softap_ssid();
    String get_wifi_mode();
    void scan_wifi();
    String format_wifi_datalist_options();
    void attempt_sta();
    void wifi_setup();
    void wifi_connection_manager();

    // Web server
    String htmlProcessor(const String&);
    void http_setup();
    void handleNotFound(AsyncWebServerRequest*);
    void handleSettingsUpdate(AsyncWebServerRequest*);
    void handleFirmwareUpdateForm(AsyncWebServerRequest*);
    void handleFirmwareUpdate(AsyncWebServerRequest*, const String&, size_t, uint8_t*, size_t, bool);
    void handleUploadForm(AsyncWebServerRequest*);
    void handleUpload( AsyncWebServerRequest*, String, size_t, uint8_t*, size_t, bool);

    // MQTT
    void mqtt_setup();
    void mqtt_connection_manager();
    void mqtt_message_callback(char*, byte*, unsigned int);



  public:

    AsyncWebServer http;
    PubSubClient mqtt;
    String device_state;
    String mqtt_status_topic;
    String mqtt_command_topic;

    IotKernel(String, String);
    void init();
    void loop();

    void mqtt_publish_state();
};

#endif
