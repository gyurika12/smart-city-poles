#pragma once
/* ---------------------------------------------------------------------------
   ΔΙΚΤΥΟ - WiFi + MQTT ΧΩΡΙΣ ΜΠΛΟΚΑΡΙΣΜΑ

   Το loop() ΔΕΝ σταματάει ποτέ περιμένοντας δίκτυο. Μία πτώση WiFi δεν αφήνει
   την κολόνα "Unavailable" μέχρι reboot - ξανασυνδέεται μόνη της.
   --------------------------------------------------------------------------- */
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

namespace Kolona {

class Net {
public:
  void begin(const char* ssid, const char* pass,
             const char* server, uint16_t port,
             const char* user, const char* mqttPass,
             const char* clientId, const char* statusTopic,
             uint8_t willQos = 1) {
    _ssid = ssid; _pass = pass;
    _user = user; _mqttPass = mqttPass;
    _clientId = clientId; _statusTopic = statusTopic; _willQos = willQos;
    _mqtt.setServer(server, port);
    _mqtt.setKeepAlive(30);
    _mqtt.setSocketTimeout(5);
    _mqtt.setBufferSize(512);
  }

  void setCallback(void (*cb)(char*, uint8_t*, unsigned int)) { _mqtt.setCallback(cb); }
  void setOnConnect(void (*fn)())        { _onConnect = fn; }   // subscribe + republish
  void setMqttRetryMs(unsigned long ms)  { _mqttRetryMs = ms; }
  void setWifiRetryMs(unsigned long ms)  { _wifiRetryMs = ms; }

  // Μία μπλοκαρισμένη προσπάθεια ΜΟΝΟ στο setup().
  bool connectWifiBlocking(unsigned long timeoutMs = 15000) {
    Serial.print("WiFi ");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);          // χωρίς power-save -> σταθερό MQTT
    WiFi.setAutoReconnect(true);
    WiFi.begin(_ssid, _pass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
      delay(250);
      Serial.print('.');
    }
    _wifiOk = (WiFi.status() == WL_CONNECTED);
    if (_wifiOk) { Serial.print(" OK IP="); Serial.println(WiFi.localIP()); }
    else         { Serial.println(" FAILED - συνεχίζω offline"); }
    return _wifiOk;
  }

  // Καλείται σε κάθε loop(). Δεν μπλοκάρει ΠΟΤΕ.
  void loop() {
    unsigned long now = millis();

    if (WiFi.status() == WL_CONNECTED) {
      if (!_wifiOk) {
        _wifiOk = true;
        Serial.print("[WiFi] back online IP="); Serial.println(WiFi.localIP());
      }
      if (!_mqtt.connected()) {
        if (now - _lastMqttTry > _mqttRetryMs) { _lastMqttTry = now; reconnect(); }
      } else {
        _mqtt.loop();
      }
    } else {
      _wifiOk = false;
      if (now - _lastWifiTry > _wifiRetryMs) {
        _lastWifiTry = now;
        Serial.println("[WiFi] lost - reconnecting...");
        WiFi.disconnect();
        WiFi.reconnect();
      }
    }
  }

  bool reconnect() {
    if (_mqtt.connected()) return true;
    if (WiFi.status() != WL_CONNECTED) return false;
    Serial.print("MQTT...");
    bool ok = _mqtt.connect(_clientId, _user, _mqttPass, _statusTopic, _willQos, true, "offline");
    if (ok) {
      Serial.println(" connected");
      _mqtt.publish(_statusTopic, "online", true);
      if (_onConnect) _onConnect();
    } else {
      Serial.printf(" failed rc=%d\n", _mqtt.state());
    }
    return ok;
  }

  bool wifiOk() const { return _wifiOk; }
  bool mqttOk()       { return _mqtt.connected(); }
  PubSubClient& mqtt() { return _mqtt; }

private:
  WiFiClient   _wc;
  PubSubClient _mqtt{_wc};
  const char *_ssid = nullptr, *_pass = nullptr, *_user = nullptr, *_mqttPass = nullptr;
  const char *_clientId = nullptr, *_statusTopic = nullptr;
  uint8_t _willQos = 1;
  bool _wifiOk = false;
  unsigned long _lastWifiTry = 0, _lastMqttTry = 0;
  unsigned long _wifiRetryMs = 10000, _mqttRetryMs = 3000;
  void (*_onConnect)() = nullptr;
};

} // namespace Kolona
