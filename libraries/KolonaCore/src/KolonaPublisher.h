#pragma once
/* ---------------------------------------------------------------------------
   ΔΗΜΟΣΙΕΥΣΗ MQTT ΜΕ ΑΝΙΧΝΕΥΣΗ ΑΛΛΑΓΗΣ

   Στέλνει ένα topic μόνο όταν η τιμή άλλαξε πάνω από το deadband, ή αν
   πέρασαν heartbeatMs. Ίδια topics, ίδιο retain - το Home Assistant δεν
   βλέπει διαφορά, αλλά η κίνηση στον broker πέφτει ~90%.

   ΧΡΗΣΗ:
     enum { P_TEMP, P_HUM, ..., P_COUNT };
     Kolona::Publisher<P_COUNT> pub(net.mqtt());
     pub.f(P_TEMP, topicTemp, temp, 1, 0.1f);
   --------------------------------------------------------------------------- */
#include <Arduino.h>
#include <PubSubClient.h>

namespace Kolona {

// Κάθε τιμή που στέλνει ο Publisher περνάει και από εδώ (το ορίζει το
// KolonaApi, ώστε το HTTP /state να έχει τις ίδιες τιμές με το MQTT).
inline void (*publishHook)(const char* topic, const char* payload) = nullptr;

template <int N>
class Publisher {
public:
  explicit Publisher(PubSubClient& mqtt) : _m(mqtt) {}

  void setHeartbeat(unsigned long ms) { _hb = ms; }

  // Μετά από (επανα)σύνδεση: ξαναστείλε τα πάντα, ώστε να μη λείπει καμία
  // retained τιμή αν ο broker ξεκίνησε από την αρχή.
  void forceAll() { for (int i = 0; i < N; i++) _init[i] = false; }

  bool changed(int id, float v, float deadband) {
    if (id < 0 || id >= N) return false;
    unsigned long now = millis();
    if (!_init[id] || fabsf(v - _last[id]) > deadband || (now - _lastMs[id]) > _hb) {
      _init[id] = true; _last[id] = v; _lastMs[id] = now;
      return true;
    }
    return false;
  }

  void i(int id, const char* topic, int v, float deadband) {          // ακέραιος
    if (!changed(id, (float)v, deadband)) return;
    char b[16]; snprintf(b, sizeof(b), "%d", v);
    send(topic, b);
  }
  void l(int id, const char* topic, long v, float deadband) {         // long
    if (!changed(id, (float)v, deadband)) return;
    char b[16]; snprintf(b, sizeof(b), "%ld", v);
    send(topic, b);
  }
  void f(int id, const char* topic, float v, int decimals, float deadband) {
    if (!changed(id, v, deadband)) return;
    char b[16]; dtostrf(v, 0, decimals, b);
    send(topic, b);
  }
  void t(int id, const char* topic, bool state,                       // ON/OFF κτλ
         const char* onText, const char* offText) {
    if (!changed(id, state ? 1.0f : 0.0f, 0.5f)) return;
    send(topic, state ? onText : offText);
  }
  void s(int id, const char* topic, const char* text, float key) {    // κείμενο + κλειδί
    if (!changed(id, key, 0.5f)) return;
    send(topic, text);
  }

private:
  // Χωρίς σύνδεση δεν στέλνεται τίποτα στο MQTT (μετά την επανασύνδεση το
  // forceAll() τα ξαναστέλνει), αλλά η τιμή καταγράφεται για το HTTP API.
  void send(const char* topic, const char* payload) {
    if (_m.connected()) _m.publish(topic, payload, true);
    if (publishHook) publishHook(topic, payload);
  }

  PubSubClient& _m;
  float         _last[N]   = {};
  unsigned long _lastMs[N] = {};
  bool          _init[N]   = {};
  unsigned long _hb = 10000;
};

} // namespace Kolona
