# TODO

## Security

- [ ] **TLS broker verification**
  - `wifi_client_secure.setInsecure()` encrypts the connection but does not check the broker's identity
  - Attempted and dropped: a CA certificate in LittleFS works, but needs NTP on ESP8266 (validity dates) and breaks when the broker's CA changes; not worth the complexity for now
