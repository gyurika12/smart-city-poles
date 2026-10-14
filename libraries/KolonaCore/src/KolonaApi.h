#pragma once
/* ---------------------------------------------------------------------------
   ΤΟΠΙΚΟ HTTP API + mDNS  -  για το integration του Home Assistant

   Η κολόνα περιγράφει μόνη της ποια είναι και τι αισθητήρες έχει, ώστε το
   Home Assistant να τη στήνει με μία IP (ή αυτόματα, μέσω mDNS).

     GET  /info   ποια κολόνα είμαι + λίστα αισθητήρων (όνομα, μονάδα, topic)
     GET  /state  τελευταίες τιμές όλων των αισθητήρων + υγεία (WiFi, μνήμη)
     POST /led    mode=AUTO|ON|OFF   (αν οριστεί KOLONA_API_TOKEN στο
                  secrets.h, θέλει header  Authorization: Bearer <token>)

   mDNS: kolona-<id>.local  +  υπηρεσία _kolona._tcp (TXT: id, api, fw)

   Οι τιμές ΔΕΝ ξαναδιαβάζονται από τους αισθητήρες: ο Publisher, κάθε φορά
   που στέλνει ένα topic, το καταγράφει και εδώ (publishHook). Άρα το API
   δείχνει ακριβώς ό,τι στέλνει και το MQTT - και συνεχίζει να ενημερώνεται
   ακόμη κι όταν ο broker είναι εκτός.

   ΧΡΗΣΗ (στο setup, ΠΡΙΝ το net.begin):
     api.identity("col1", "Κολώνα 1", MY_INDEX);
     api.mqtt(net.mqtt(), topicStatus, topicWaveTx);
     api.sensor("temperature", "Θερμοκρασία", topicTemp, "°C", "temperature");
     api.binary("presence", "Παρουσία", topicLaserTrig, "ON", "OFF", "occupancy");
     api.ledControl(topicLedCmd, topicLedMode, onLedHttp);
     api.begin();
   και στο loop:  api.loop();
   --------------------------------------------------------------------------- */
#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
// Το TFT_eSPI (COL1/COL3) ορίζει FS_NO_GLOBALS, οπότε τα FS/File δεν είναι στο
// global namespace όπως τα περιμένει το WebServer.h. Τα φέρνουμε εδώ.
using fs::FS;
using fs::File;
#include <WebServer.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include "KolonaPublisher.h"
#include "KolonaSafety.h"

#ifndef KOLONA_API_TOKEN
#define KOLONA_API_TOKEN ""          // κενό = /led χωρίς κωδικό (μόνο για κλειστό δίκτυο)
#endif

#define KOLONA_API_VERSION 1

#ifndef KOLONA_FW_VERSION
#define KOLONA_FW_VERSION "dev"
#endif

namespace Kolona {

/* Μικρός συγγραφέας JSON πάνω σε σταθερό buffer: καμία δέσμευση μνήμης ανά
   αίτημα (το heap μένει ενιαίο - βλ. slideshow). Βάζει μόνος του τα κόμματα. */
class JsonOut {
public:
  JsonOut(char* buf, size_t size) : _b(buf), _n(size) { _b[0] = '\0'; }

  void open(char c)  { sep(); put(c); _need = false; }
  void close(char c) { put(c); _need = true; }
  void key(const char* k)               { sep(); str(k); put(':'); _need = false; }
  void val(const char* s)               { sep(); if (s) str(s); else raw("null"); _need = true; }
  void val(long v)                      { char t[16]; snprintf(t, sizeof(t), "%ld", v); sep(); raw(t); _need = true; }
  void val(bool b)                      { sep(); raw(b ? "true" : "false"); _need = true; }
  void num(double v, int decimals)      { char t[24]; snprintf(t, sizeof(t), "%.*f", decimals, v); sep(); raw(t); _need = true; }
  void kv(const char* k, const char* s) { key(k); val(s); }
  void kv(const char* k, long v)        { key(k); val(v); }
  void kv(const char* k, int v)         { key(k); val((long)v); }
  void kv(const char* k, bool b)        { key(k); val(b); }

  bool   ok() const     { return !_overflow; }
  size_t length() const { return _len; }

private:
  void sep() { if (_need) put(','); _need = false; }
  void raw(const char* s) { while (*s) put(*s++); }
  void str(const char* s) {
    put('"');
    for (; *s; s++) {
      char c = *s;
      if (c == '"' || c == '\\') { put('\\'); put(c); }
      else if ((uint8_t)c < 0x20) { char e[8]; snprintf(e, sizeof(e), "\\u%04x", c); raw(e); }
      else put(c);                       // UTF-8 (ελληνικά) περνάει αυτούσιο
    }
    put('"');
  }
  void put(char c) {
    if (_len + 1 >= _n) { _overflow = true; return; }
    _b[_len++] = c; _b[_len] = '\0';
  }

  char*  _b;
  size_t _n, _len = 0;
  bool   _need = false, _overflow = false;
};


class Api {
public:
  static const int MAX_ENTITIES = 40;
  static const int VALUE_LEN    = 40;

  /* ---- Δήλωση (στο setup) ---- */
  void identity(const char* id, const char* name, int index,
                const char* model = "Smart Pole ESP32-S3") {
    _id = id; _name = name; _index = index; _model = model;
    snprintf(_host, sizeof(_host), "kolona-%s", id);
  }

  void mqtt(PubSubClient& client, const char* statusTopic, const char* waveTopic) {
    _mqtt = &client; _statusTopic = statusTopic; _waveTopic = waveTopic;
  }

  // Αριθμητική μέτρηση. scale: π.χ. 0.01 όταν το topic στέλνει Pa και η
  // μονάδα είναι hPa (το Home Assistant πολλαπλασιάζει με scale).
  void sensor(const char* key, const char* name, const char* topic,
              const char* unit = nullptr, const char* deviceClass = nullptr,
              float scale = 1.0f, const char* stateClass = "measurement") {
    Entity* e = add(key, name, topic);
    if (!e) return;
    e->platform = "sensor"; e->unit = unit; e->deviceClass = deviceClass;
    e->stateClass = stateClass; e->scale = scale;
  }

  // Ναι/όχι. payloadOn/Off = ό,τι στέλνει το topic (π.χ. "RAIN"/"DRY").
  void binary(const char* key, const char* name, const char* topic,
              const char* payloadOn = "ON", const char* payloadOff = "OFF",
              const char* deviceClass = nullptr) {
    Entity* e = add(key, name, topic);
    if (!e) return;
    e->platform = "binary_sensor"; e->payloadOn = payloadOn;
    e->payloadOff = payloadOff; e->deviceClass = deviceClass;
  }

  // Κείμενο (π.χ. η λειτουργία του LED: AUTO/ON/OFF).
  void text(const char* key, const char* name, const char* topic) {
    Entity* e = add(key, name, topic);
    if (e) e->platform = "text";
  }

  // Έλεγχος LED: ίδιο command topic με το MQTT, και HTTP POST /led.
  void ledControl(const char* commandTopic, const char* modeTopic,
                  void (*onCommand)(const char* mode)) {
    _ledCmdTopic = commandTopic; _ledModeTopic = modeTopic; _onLed = onCommand;
  }

  void begin() {
    if (_host[0]) WiFi.setHostname(_host);       // πριν συνδεθεί το WiFi
    _self = this;
    publishHook = &Api::hook;

    static const char* headers[] = { "Authorization" };
    _server.collectHeaders(headers, 1);
    _server.on("/info",  HTTP_GET,  [this]() { handleInfo(); });
    _server.on("/state", HTTP_GET,  [this]() { handleState(); });
    _server.on("/led",   HTTP_POST, [this]() { handleLed(); });
    _server.onNotFound([this]() { sendError(404, "not found"); });
  }

  /* ---- Στο loop: δεν μπλοκάρει ---- */
  void loop() {
    bool up = (WiFi.status() == WL_CONNECTED);
    if (up && !_netUp) onNetworkUp();
    _netUp = up;
    if (_started) _server.handleClient();
  }

  /* Καλείται από τον Publisher για κάθε τιμή που στέλνει. */
  void record(const char* topic, const char* payload) {
    for (int i = 0; i < _count; i++) {
      if (strcmp(_e[i].topic, topic) == 0) {
        strlcpy(_e[i].value, payload, VALUE_LEN);
        _e[i].ms = millis();
        return;
      }
    }
  }

private:
  struct Entity {
    const char* key = nullptr;
    const char* name = nullptr;
    const char* topic = nullptr;
    const char* platform = "sensor";
    const char* unit = nullptr;
    const char* deviceClass = nullptr;
    const char* stateClass = nullptr;
    const char* payloadOn = nullptr;
    const char* payloadOff = nullptr;
    float scale = 1.0f;
    char value[VALUE_LEN] = {0};
    unsigned long ms = 0;               // 0 = δεν έχει έρθει ακόμη τιμή
  };

  Entity* add(const char* key, const char* name, const char* topic) {
    if (_count >= MAX_ENTITIES) {
      Serial.printf("[API] πάρα πολλοί αισθητήρες - αγνοώ το %s\n", key);
      return nullptr;
    }
    Entity* e = &_e[_count++];
    e->key = key; e->name = name; e->topic = topic;
    return e;
  }

  static void hook(const char* topic, const char* payload) {
    if (_self) _self->record(topic, payload);
  }

  void onNetworkUp() {
    if (!_started) { _server.begin(); _started = true; }
    // Το mDNS ξαναστήνεται σε κάθε επανασύνδεση του WiFi.
    MDNS.end();
    if (MDNS.begin(_host)) {
      MDNS.addService("kolona", "tcp", 80);
      MDNS.addServiceTxt("kolona", "tcp", "id", _id);
      MDNS.addServiceTxt("kolona", "tcp", "api", String(KOLONA_API_VERSION));
      MDNS.addServiceTxt("kolona", "tcp", "fw", KOLONA_FW_VERSION);
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[API] http://%s.local  (%s)\n", _host, WiFi.localIP().toString().c_str());
    } else {
      Serial.println("[API] mDNS FAIL - η κολόνα απαντάει μόνο στην IP της");
    }
  }

  bool authorized() {
    const char* token = KOLONA_API_TOKEN;
    if (!token[0]) return true;
    String h = _server.header("Authorization");
    return h.length() == strlen(token) + 7 && h.startsWith("Bearer ") && h.endsWith(token);
  }

  void send(int code, JsonOut& j) {
    if (!j.ok()) { sendError(500, "response too large"); return; }
    _server.sendHeader("Cache-Control", "no-store");
    _server.send_P(code, "application/json", _buf, j.length());
  }

  void sendError(int code, const char* msg) {
    JsonOut j(_buf, sizeof(_buf));
    j.open('{'); j.kv("error", msg); j.close('}');
    _server.send_P(code, "application/json", _buf, j.length());
  }

  void handleInfo() {
    Kolona::beat();
    JsonOut j(_buf, sizeof(_buf));
    j.open('{');
    j.kv("api", (long)KOLONA_API_VERSION);
    j.kv("id", _id);
    j.kv("name", _name);
    j.kv("index", _index);
    j.kv("model", _model);
    j.kv("manufacturer", "Smart City Poles");
    j.kv("fw", KOLONA_FW_VERSION);
    j.kv("mac", WiFi.macAddress().c_str());
    j.kv("hostname", _host);
    j.kv("auth", KOLONA_API_TOKEN[0] != '\0');

    j.key("mqtt"); j.open('{');
      j.kv("status_topic", _statusTopic);
      j.kv("wave_topic", _waveTopic);
      j.kv("payload_online", "online");
      j.kv("payload_offline", "offline");
    j.close('}');

    j.key("led"); j.open('{');
      j.kv("command_topic", _ledCmdTopic);
      j.kv("state_topic", _ledModeTopic);
      j.key("options"); j.open('['); j.val("AUTO"); j.val("ON"); j.val("OFF"); j.close(']');
      j.kv("http", _onLed ? "/led" : nullptr);
    j.close('}');

    j.key("entities"); j.open('[');
    for (int i = 0; i < _count; i++) {
      const Entity& e = _e[i];
      j.open('{');
      j.kv("key", e.key);
      j.kv("name", e.name);
      j.kv("platform", e.platform);
      j.kv("topic", e.topic);
      if (e.unit)        j.kv("unit", e.unit);
      if (e.deviceClass) j.kv("device_class", e.deviceClass);
      if (e.stateClass)  j.kv("state_class", e.stateClass);
      if (e.payloadOn)   j.kv("payload_on", e.payloadOn);
      if (e.payloadOff)  j.kv("payload_off", e.payloadOff);
      if (e.scale != 1.0f) { j.key("scale"); j.num(e.scale, 6); }
      j.close('}');
    }
    j.close(']');
    j.close('}');
    send(200, j);
  }

  void handleState() {
    Kolona::beat();
    unsigned long now = millis();
    JsonOut j(_buf, sizeof(_buf));
    j.open('{');
    j.kv("id", _id);
    j.kv("uptime_s", (long)(now / 1000));
    j.kv("rssi", (long)WiFi.RSSI());
    j.kv("ip", WiFi.localIP().toString().c_str());
    j.kv("heap_free", (long)ESP.getFreeHeap());
    j.kv("heap_max_block", (long)largestFreeBlock());
    j.kv("mqtt", _mqtt ? _mqtt->connected() : false);

    j.key("values"); j.open('{');
    for (int i = 0; i < _count; i++) {
      const Entity& e = _e[i];
      j.key(e.key);
      if (!e.ms) { j.val((const char*)nullptr); continue; }
      j.open('{');
      j.kv("value", e.value);
      j.kv("age_s", (long)((now - e.ms) / 1000));
      j.close('}');
    }
    j.close('}');
    j.close('}');
    send(200, j);
  }

  void handleLed() {
    if (!_onLed)       { sendError(404, "no led control"); return; }
    if (!authorized()) { sendError(401, "unauthorized"); return; }
    char mode[8];
    strlcpy(mode, _server.arg("mode").c_str(), sizeof(mode));
    for (char* p = mode; *p; p++) *p = toupper((int)*p);
    if (strcmp(mode, "AUTO") && strcmp(mode, "ON") && strcmp(mode, "OFF")) {
      sendError(400, "mode must be AUTO, ON or OFF");
      return;
    }
    _onLed(mode);
    JsonOut j(_buf, sizeof(_buf));
    j.open('{'); j.kv("ok", true); j.kv("mode", mode); j.close('}');
    send(200, j);
  }

  WebServer _server{80};
  Entity _e[MAX_ENTITIES];
  int _count = 0;
  char _buf[4096];                       // ένας buffer για όλες τις απαντήσεις
  char _host[32] = {0};

  const char *_id = "", *_name = "", *_model = "";
  int _index = 0;
  PubSubClient* _mqtt = nullptr;
  const char *_statusTopic = nullptr, *_waveTopic = nullptr;
  const char *_ledCmdTopic = nullptr, *_ledModeTopic = nullptr;
  void (*_onLed)(const char*) = nullptr;
  bool _started = false, _netUp = false;

  static inline Api* _self = nullptr;
};

} // namespace Kolona
