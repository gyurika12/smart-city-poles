#pragma once
/* ---------------------------------------------------------------------------
   ΦΩΤΙΣΜΟΣ + ΚΥΜΑ - κοινό και στις 3 κολόνες

   4 ΕΠΙΠΕΔΑ (σε λειτουργία AUTO):
       ΜΕΡΑ .................   0%   (PWM_OFF)
       ΝΥΧΤΑ, κανείς ........  50%   (PWM_DIM)
       ΓΕΙΤΟΝΑΣ ανίχνευσε ...  78%   (PWM_WAVE)  - "κύμα", κρατάει waveHold
       ΕΓΩ ανίχνευσα ........ 100%   (PWM_MAX)   - κρατάει motionHold

   Το άναμμα/σβήσιμο γίνεται με ράμπα (βήμα ανά κλήση) ώστε να μην "πετάγεται".
   --------------------------------------------------------------------------- */
#include <Arduino.h>

namespace Kolona {

enum LedMode { LED_AUTO, LED_ON, LED_OFF };

class Led {
public:
  static const int PWM_OFF  = 0;
  static const int PWM_DIM  = 128;   // 50%
  static const int PWM_WAVE = 200;   // ~78%
  static const int PWM_MAX  = 255;   // 100%

  void begin(int pin, int freq = 5000, int res = 8, bool activeLow = false) {
    _pin = pin; _activeLow = activeLow;
    ledcAttach(pin, freq, res);
    writeRaw(PWM_OFF);
  }

  void setHold(unsigned long motionMs, unsigned long waveMs) {
    _motionHold = motionMs; _waveHold = waveMs;
  }
  void setRampStep(int step)          { _step = step; }
  void setWaveSender(void (*fn)())    { _sendWave = fn; }

  /* Η ΒΑΣΙΚΗ ΛΟΓΙΚΗ. Καλείται σε σταθερό ρυθμό (π.χ. κάθε 20ms) ώστε η ράμπα
     να τρέχει με την ίδια ταχύτητα σε όλες τις κολόνες.                    */
  void update(bool dark, bool laserTrig) {
    unsigned long now = millis();
    int target = PWM_OFF;

    if (_mode == LED_ON) {
      target = PWM_MAX;
    } else if (_mode == LED_OFF) {
      target = PWM_OFF;
    } else if (!dark) {
      target = PWM_OFF;                               // μέρα -> σβηστό
    } else {
      if (laserTrig) {
        _motionUntil = now + _motionHold;
        _waveUntil   = now + _waveHold;
        // κύμα στην αρχή ΚΑΙ ξανά κάθε 1s όσο κρατάει η ανίχνευση, ώστε ο
        // γείτονας να μένει στο 78% όσο υπάρχει κάποιος μπροστά.
        if (!_lastTrig || now - _lastWaveSent > 1000) {
          if (_sendWave) _sendWave();
          _lastWaveSent = now;
        }
      }
      if (laserTrig || now < _motionUntil) target = PWM_MAX;
      else if (now < _waveUntil)           target = PWM_WAVE;
      else                                 target = PWM_DIM;
    }

    _lastTrig = laserTrig;
    ramp(target);
  }

  // Κύμα από ΓΕΙΤΟΝΙΚΗ κολόνα -> προ-άναμμα στο 78%.
  void onNeighbourWave() {
    if (_mode != LED_AUTO) return;
    _waveUntil = millis() + _waveHold;
  }

  void    setMode(LedMode m) { _mode = m; }
  LedMode mode() const       { return _mode; }
  int     pwm() const        { return _pwm; }
  bool    on() const         { return _pwm > 0; }

  const char* modeString() const {
    switch (_mode) {
      case LED_ON:  return "ON";
      case LED_OFF: return "OFF";
      default:      return "AUTO";
    }
  }

  /* Διαβάζει εντολή MQTT ("ON"/"1", "OFF"/"0", "AUTO").
     unknownIsAuto=true -> ό,τι άλλο το θεωρεί AUTO (συμπεριφορά COL1).   */
  bool applyCommand(const char* msg, bool unknownIsAuto = false) {
    if      (!strcmp(msg, "ON")   || !strcmp(msg, "1")) _mode = LED_ON;
    else if (!strcmp(msg, "OFF")  || !strcmp(msg, "0")) _mode = LED_OFF;
    else if (!strcmp(msg, "AUTO"))                      _mode = LED_AUTO;
    else if (unknownIsAuto)                             _mode = LED_AUTO;
    else return false;
    return true;
  }

private:
  void ramp(int target) {
    target = constrain(target, PWM_OFF, PWM_MAX);
    if (target > _pwm)      _pwm += min(_step, target - _pwm);
    else if (target < _pwm) _pwm -= min(_step, _pwm - target);
    writeRaw(_pwm);
  }
  void writeRaw(int v) { ledcWrite(_pin, _activeLow ? (PWM_MAX - v) : v); }

  int  _pin = -1, _pwm = 0, _step = 8;
  bool _activeLow = false;
  LedMode _mode = LED_AUTO;
  unsigned long _motionHold = 7000, _waveHold = 2500;
  unsigned long _motionUntil = 0, _waveUntil = 0, _lastWaveSent = 0;
  bool _lastTrig = false;
  void (*_sendWave)() = nullptr;
};


/* Βοηθός για το πρωτόκολλο του κύματος πάνω από MQTT.
   Το μήνυμα είναι "colN". Δέχεται μόνο ΓΕΙΤΟΝΙΚΗ κολόνα (|N - myIndex| == 1). */
inline bool waveIsFromNeighbour(const char* upperMsg, int myIndex) {
  if (strncmp(upperMsg, "COL", 3) != 0) return false;
  int sender = atoi(upperMsg + 3);
  return (sender != 0 && abs(sender - myIndex) == 1);
}

/* Κανονικοποιεί ένα MQTT payload σε κεφαλαία, χωρίς κενά/newline στο τέλος. */
inline void normalizePayload(char* dst, size_t dstSize,
                             const uint8_t* payload, unsigned int length) {
  unsigned int n = (length < dstSize - 1) ? length : dstSize - 1;
  for (unsigned int i = 0; i < n; i++) dst[i] = toupper((int)payload[i]);
  dst[n] = '\0';
  while (n > 0 && (dst[n-1] == ' ' || dst[n-1] == '\r' || dst[n-1] == '\n')) dst[--n] = '\0';
}

} // namespace Kolona
