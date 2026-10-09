#pragma once
/* ---------------------------------------------------------------------------
   ΑΝΙΧΝΕΥΣΗ + ΥΣΤΕΡΗΣΗ - κοινά και στις 3 κολόνες
   --------------------------------------------------------------------------- */
#include <Arduino.h>

namespace Kolona {

/*  Διακόπτης με δύο κατώφλια (anti-flicker).
    Χαμηλή τιμή = "ναι" (σκοτάδι), υψηλή = "όχι" (φως).
    Ανάμεσα στα δύο κατώφλια δεν αλλάζει τίποτα -> δεν τρεμοπαίζει.       */
class Hysteresis {
public:
  void configure(float onBelow, float offAbove, bool initial = false) {
    _on = onBelow; _off = offAbove; _v = initial;
  }
  bool update(float value) {
    if (value < _on)       _v = true;
    else if (value > _off) _v = false;
    return _v;
  }
  // Όταν η μέτρηση μπορεί να είναι άκυρη (π.χ. lux = -1): κρατάει την προηγούμενη.
  bool updateIfValid(float value, bool valid) { return valid ? update(value) : _v; }
  bool value() const { return _v; }

private:
  float _on = 0, _off = 0;
  bool  _v = false;
};


/*  Ανίχνευση διέλευσης με laser TOF.
    Ενεργοποιείται όταν κάτι έρθει πιο κοντά από triggerMm, αλλά μόνο αν
    επιμείνει για nearConfirm συνεχόμενα δείγματα (anti-θόρυβος).          */
class Detect {
public:
  void configure(int triggerMm, int nearConfirm,
                 int minValidMm = 30, int maxValidMm = 1800) {
    _trigger = triggerMm; _confirm = nearConfirm;
    _min = minValidMm;    _max = maxValidMm;
  }

  bool update(int tofMm) {
    if (tofMm < _min || tofMm > _max) { _near = 0; return false; }  // θόρυβος ή κενό
    _lastValid = tofMm;
    if (tofMm < _trigger) {
      if (_near < _confirm) _near++;
      return (_near >= _confirm);
    }
    _near = 0;
    return false;
  }

  int lastValidMm() const { return _lastValid; }   // για banner/debug

private:
  int _trigger = 200, _confirm = 2, _min = 30, _max = 1800;
  int _near = 0, _lastValid = -1;
};

} // namespace Kolona
