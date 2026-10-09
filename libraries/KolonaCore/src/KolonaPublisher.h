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
    _m.publish(topic, b, true);
  }
  void l(int id, const char* topic, long v, float deadband) {         // long
    if (!changed(id, (float)v, deadband)) return;
    char b[16]; snprintf(b, sizeof(b), "%ld", v);
    _m.publish(topic, b, true);
  }
  void f(int id, const char* topic, float v, int decimals, float deadband) {
    if (!changed(id, v, deadband)) return;
    char b[16]; dtostrf(v, 0, decimals, b);
    _m.publish(topic, b, true);
  }
  void t(int id, const char* topic, bool state,                       // ON/OFF κτλ
         const char* onText, const char* offText) {
    if (!changed(id, state ? 1.0f : 0.0f, 0.5f)) return;
    _m.publish(topic, state ? onText : offText, true);
  }
  void s(int id, const char* topic, const char* text, float key) {    // κείμενο + κλειδί
    if (!changed(id, key, 0.5f)) return;
    _m.publish(topic, text, true);
  }

private:
  PubSubClient& _m;
  float         _last[N]   = {};
  unsigned long _lastMs[N] = {};
  bool          _init[N]   = {};
  unsigned long _hb = 10000;
};

} // namespace Kolona
