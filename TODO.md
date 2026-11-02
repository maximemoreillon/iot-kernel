# TODO

## Security

- [ ] **Optional admin password** (planned for v0.4.0)
  - Add an `admin_password` field to `config.json` and the settings form
  - When set, require HTTP Basic auth on the whole web server (`setAuthentication`); when empty, keep the current behaviour
  - Use it as the WiFi password of the access point as well (currently an open network, so anyone nearby can reconfigure the device when it falls back to AP mode)
  - Requires re-uploading the GUI to existing devices
- [ ] **TLS broker verification**
  - `wifi_client_secure.setInsecure()` encrypts the connection but does not check the broker's identity
  - Add an optional CA certificate or fingerprint to the config; keep `setInsecure()` as the default so existing setups keep working
- [ ] **Clearing a saved password**
  - An empty password field now keeps the current password, so switching to an open WiFi network requires editing `config.json`
  - Add e.g. a "clear password" checkbox to the settings form

## Bugs / improvements

- [ ] **Unused last-will JSON** (`mqttUtils.cpp`, `mqtt_connection_manager`)
  - A JSON payload is built but `"offline"` is sent as the last will instead; either use it or remove it
- [ ] **No WiFi recovery after falling back to AP mode** (`wifiUtils.cpp`)
  - If the router is down at boot, the device stays in AP mode until rebooted; retry STA periodically
  - With no SSID configured, it still waits the full 20 s (`WIFI_STA_CONNECTION_TIMEOUT`) before starting the AP
- [ ] **No callback for MQTT commands**
  - The MQTT callback changes `device_state`, but the sketch is not notified and has to poll it
  - Add e.g. `onStateChange(std::function<void(const String&)>)`
- [ ] **Template values are not HTML-escaped** (`htmlProcessor`)
  - A `"` in the nickname, SSID, etc. breaks the settings form
- [ ] **Static locals in connection managers**
  - `static` variables in `mqtt_connection_manager` and `wifi_connection_manager` are shared across instances; fine with a single `IotKernel`, but they could be made class members
