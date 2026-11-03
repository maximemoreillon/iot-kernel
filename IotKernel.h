#ifndef IotKernel_h
#define IotKernel_h

#include <PubSubClient.h> // MQTT
#include <WiFiClientSecure.h> // Wifi client, used by MQTT
#include <ESPAsyncWebServer.h>
#include <DNSServer.h> // DNS server to redirect wifi clients to the web server
#include <ArduinoJson.h> // JSON, used for the formatting of messages sent to the server
#include <LittleFS.h>
#include <Ticker.h>
#include <functional>

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


#define IOT_KERNEL_VERSION "0.4.0"

#define WIFI_STA_CONNECTION_TIMEOUT 20000
#define WIFI_AP_STA_RETRY_PERIOD 300000 // While in AP mode, retry the configured WiFi every 5 minutes
#define WIFI_AP_MIN_PASSWORD_LENGTH 8 // WPA2 requirement
#define WIFI_AP_IP IPAddress(192, 168, 4, 1)

#define MQTT_RECONNECT_PERIOD 5000
#define MQTT_QOS 1
#define MQTT_RETAIN true

#define DNS_PORT 53
#define WEB_SERVER_PORT 80
#define WEB_SERVER_ADMIN_USERNAME "admin"



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
  String admin_password;
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

    std::function<void(const String&)> state_change_callback;
    std::function<void()> wifi_connect_callback;
    std::function<void()> wifi_disconnect_callback;
    std::function<void()> mqtt_connect_callback;
    std::function<void()> mqtt_disconnect_callback;

    // Connection state tracking
    boolean wifi_last_connection_state;
    boolean mqtt_last_connection_state;
    unsigned long mqtt_last_connection_attempt;
    unsigned long wifi_ap_since; // When the AP was started (or STA last retried)
    unsigned long wifi_sta_retry_start; // When the current STA retry from AP mode started, 0 if none

    // OTA safeguards
    // NOTE: if lastOtaWriteTime is unsigned long, millis() - lastOtaWriteTime will yield large number
    long lastOtaWriteTime;
    AsyncWebServerRequest* otaRequest; // request owning the update in progress, or nullptr

    // Misc
    String get_chip_id();
    String get_device_name();
    boolean is_unset(String);
    boolean has_admin_password();

    // SPIFFS
    void spiffs_setup();
    void get_config_from_spiffs();

    // Wifi
    String get_hostname();
    boolean wifi_connected();
    String get_softap_ssid();
    String get_wifi_mode();
    void wifi_begin();
    void attempt_sta();
    void start_ap();
    void wifi_setup();
    void wifi_connection_manager();
    void wifi_ap_retry_manager();

    // Web server
    void http_setup();
    String htmlProcessor(const String&);
    String htmlEscape(const String&);
    boolean isSameOrigin(AsyncWebServerRequest*);
    boolean isAuthorized(AsyncWebServerRequest*);
    void requireAuthentication(AsyncWebHandler&);
    String updatedPassword(AsyncWebServerRequest*, const char*, const char*, const String&);
    String sanitizeUploadFilename(const String&);
    void handleNotFound(AsyncWebServerRequest*);
    void handleSettingsUpdate(AsyncWebServerRequest*);
    void handleFirmwareUpdateForm(AsyncWebServerRequest*);
    void handleFirmwareUpdate(AsyncWebServerRequest*, const String&, size_t, uint8_t*, size_t, bool);
    void handleFirmwareUpdateResult(AsyncWebServerRequest*);
    void handleUploadForm(AsyncWebServerRequest*);
    void handleUpload( AsyncWebServerRequest*, String, size_t, uint8_t*, size_t, bool);

    // MQTT
    void mqtt_setup();
    void set_state_from_command(const char*);
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

    IotKernel(String type, String version);

    void init(String state = "off");
    void loop();

    void mqtt_publish_state();
    void mqtt_publish_available();
    void mqtt_publish_info();

    // Called when the state is changed by an MQTT command
    void onStateChange(std::function<void(const String&)> callback);

    // Called when the connection to the configured WiFi is established or lost
    void onWifiConnect(std::function<void()> callback);
    void onWifiDisconnect(std::function<void()> callback);

    // Called when the connection to the MQTT broker is established or lost
    // NOTE: onMqttConnect is called after subscribing and publishing state, availability and info
    void onMqttConnect(std::function<void()> callback);
    void onMqttDisconnect(std::function<void()> callback);
};

#endif
