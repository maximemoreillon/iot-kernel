#ifndef IotKernel_h
#define IotKernel_h

#include <PubSubClient.h> // MQTT
#include <WiFiClientSecure.h> // Wifi client, used by MQTT
#include <ESPAsyncWebServer.h>
#include <DNSServer.h> // DNS server to redirect wifi clients to the web server
#include <ArduinoJson.h> // JSON, used for the formatting of messages sent to the server
#include <LittleFS.h>
#include <Ticker.h>

// Select ESP32 or ESP8266
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#include <Update.h>

#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <Updater.h>
#endif


#define IOT_KERNEL_VERSION "0.3.2"

#define WIFI_STA_CONNECTION_TIMEOUT 20000
#define WIFI_AP_IP IPAddress(192, 168, 4, 1)

#define MQTT_RECONNECT_PERIOD 5000
#define MQTT_QOS 1
#define MQTT_RETAIN true

#define DNS_PORT 53
#define WEB_SERVER_PORT 80



// Custom structure to handle config
struct WifiConfig {
  String ssid;
  String password;
};

struct MqttBrokerConfig {
  String host;
  uint port;
  String secure;
};

struct MqttConfig {
  MqttBrokerConfig broker;
  String username;
  String password;
};

struct DeviceConfig {
  String nickname;
  String hostname;
  WifiConfig wifi;
  MqttConfig mqtt;
};

class IotKernel {

  private:

    WiFiClient wifi_client;
    WiFiClientSecure wifi_client_secure;
    DNSServer dns_server;
    Ticker rebootTimer;

    DeviceConfig config;

    // OTA safeguards
    boolean otaInProgress;
    unsigned long lastOtaWriteTime;

    // Misc
    String get_chip_id();
    String get_device_name();
    boolean is_unset(String);

    // SPIFFS
    void spiffs_setup();
    void get_config_from_spiffs();

    // Wifi
    String get_hostname();
    boolean wifi_connected();
    String get_softap_ssid();
    String get_wifi_mode();
    void attempt_sta();
    void wifi_setup();
    void wifi_connection_manager();

    // Web server
    void http_setup();
    String htmlProcessor(const String&);
    void handleNotFound(AsyncWebServerRequest*);
    void handleSettingsUpdate(AsyncWebServerRequest*);
    void handleFirmwareUpdateForm(AsyncWebServerRequest*);
    void handleFirmwareUpdate(AsyncWebServerRequest*, const String&, size_t, uint8_t*, size_t, bool);
    void handleFirmwareUpdateResult(AsyncWebServerRequest*);
    void handleUploadForm(AsyncWebServerRequest*);
    void handleUpload( AsyncWebServerRequest*, String, size_t, uint8_t*, size_t, bool);

    // MQTT
    void mqtt_setup();
    void handle_mqtt();
    void mqtt_connection_manager();
    void mqtt_message_callback(char*, byte*, unsigned int);

  public:

    AsyncWebServer http;
    PubSubClient mqtt;

    String device_state;

    String mqtt_base_topic;
    String mqtt_state_topic;
    String mqtt_command_topic;
    String mqtt_info_topic;
    String mqtt_availability_topic;

    String device_type;
    String device_name;
    String firmware_version;

    IotKernel(String, String);

    void init();
    void loop();

    void mqtt_publish_state();
    void mqtt_publish_available();
    void mqtt_publish_info();
};

#endif
