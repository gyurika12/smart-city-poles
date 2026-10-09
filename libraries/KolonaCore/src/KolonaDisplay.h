#pragma once
/* ---------------------------------------------------------------------------
   ΟΘΟΝΗ - κυλιόμενο banner + slideshow εικόνων από το Home Assistant
   (μόνο COL1 και COL3 - η COL2 δεν έχει οθόνη)

   ΚΡΙΣΙΜΟ ΓΙΑ ΤΗ ΣΤΑΘΕΡΟΤΗΤΑ: ο buffer εικόνας δεσμεύεται ΜΙΑ ΦΟΡΑ στο boot
   (όσο η μνήμη είναι ακόμα ενιαία) και δεν ελευθερώνεται ποτέ. Η παλιά έκδοση
   έκανε malloc/free 200KB εν ώρα λειτουργίας -> κατακερματισμός -> κρασάρισμα.
   Επίσης: ΜΗΔΕΝ String - σταθεροί char πίνακες, καμία δέσμευση μνήμης.
   --------------------------------------------------------------------------- */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include "KolonaSafety.h"

/* ---------------------------------------------------------------------------
   ΕΛΕΓΧΟΣ ΟΤΙ ΕΙΝΑΙ ΕΝΕΡΓΟ ΤΟ ΣΩΣΤΟ setup ΤΟΥ TFT_eSPI

   Κάθε κολόνα έχει διαφορετική οθόνη:
       COL1 -> User_Setup2.h   (DC=7, RST=3, MISO αχρησιμοποίητο, 20 MHz)
       COL3 -> User_Setup.h    (DC=2, RST=4, MISO=13,              1 MHz)

   Η επιλογή ΔΕΝ μπορεί να γίνει από εδώ: το TFT_eSPI.cpp μεταγλωττίζεται
   ξεχωριστά από το sketch και δεν βλέπει τα #define του sketch. Γίνεται από
   το αρχείο build_opt.h μέσα στον φάκελο του sketch (μόνο η COL1 το χρειάζεται).

   Αυτό που κάνουμε εδώ είναι να ΕΠΑΛΗΘΕΥΣΟΥΜΕ ότι έγινε σωστά. Το sketch
   δηλώνει τι περιμένει, και αν δεν ταιριάζει η μεταγλώττιση σταματάει με
   καθαρό μήνυμα - αντί να ανέβει firmware που αφήνει μαύρη οθόνη.
   --------------------------------------------------------------------------- */
#if defined(KOLONA_EXPECT_TFT_DC) && (TFT_DC != KOLONA_EXPECT_TFT_DC)
  #error "ΛΑΘΟΣ setup TFT_eSPI για αυτη την κολονα (TFT_DC). Δες KolonaDisplay.h: η COL1 θελει -DKOLONA_TFT_SETUP2 απο το build_opt.h του φακελου της, η COL3 δεν θελει τιποτα. Σβησε και τον φακελο build και ξαναμεταγλωττισε."
#endif
#if defined(KOLONA_EXPECT_TFT_RST) && (TFT_RST != KOLONA_EXPECT_TFT_RST)
  #error "ΛΑΘΟΣ setup TFT_eSPI για αυτη την κολονα (TFT_RST). Δες KolonaDisplay.h."
#endif

namespace Kolona {

/* --------------------------- Κυλιόμενο banner --------------------------- */
class Banner {
public:
  bool begin(TFT_eSPI& tft, int y, int w = 240, int h = 24) {
    _tft = &tft; _y = y; _w = w;
    _spr = new TFT_eSprite(&tft);
    _spr->setColorDepth(16);
    _ok = (_spr->createSprite(w, h) != nullptr);
    if (_ok) { _spr->setTextFont(1); _spr->setTextSize(1); }
    _x = w;
    return _ok;
  }

  void setText(const char* s) {
    strncpy(_text, s, sizeof(_text) - 1);
    _text[sizeof(_text) - 1] = '\0';
    _textW = strlen(_text) * 6;          // font 1, size 1 -> 6px/χαρακτήρα
  }

  void tick(int stepPx = 4) {
    if (_ok) {
      _spr->fillSprite(TFT_BLACK);
      _spr->setTextColor(TFT_YELLOW, TFT_BLACK);
      _spr->setCursor(_x, 8);
      _spr->print(_text);
      _spr->pushSprite(0, _y);           // ένα γρήγορο "σπρώξιμο" -> χωρίς flicker
    } else {
      _tft->fillRect(0, _y, _w, 24, TFT_BLACK);
      _tft->setTextFont(1); _tft->setTextSize(1);
      _tft->setTextColor(TFT_YELLOW, TFT_BLACK);
      _tft->setCursor(_x, _y + 6);
      _tft->print(_text);
    }
    _x -= stepPx;
    if (_x < -_textW) _x = _w;
  }

private:
  TFT_eSPI*    _tft = nullptr;
  TFT_eSprite* _spr = nullptr;
  bool _ok = false;
  int  _y = 0, _w = 240, _x = 240, _textW = 0;
  char _text[96] = "";
};


/* ------------- Πεδίο πάνω μπάρας ΧΩΡΙΣ fillRect (το padding σβήνει) ------ */
inline void topBarField(TFT_eSPI& tft, int x, int y, int w,
                        const char* s, uint16_t color) {
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextPadding(w);
  tft.drawString(s, x, y);
}


/* ------------------------------- Slideshow ------------------------------- */
class Slideshow {
public:
  static const int MAX_IMAGES = 24;
  static const int NAME_LEN   = 48;

  void begin(TFT_eSPI& tft, const char* haBase, const char* imgDir,
             int x, int y, int w, int h) {
    _tft = &tft; _base = haBase; _dir = imgDir;
    _x = x; _y = y; _w = w; _h = h;
    s_self = this;
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(false);
    TJpgDec.setCallback(jpgOutput);
  }

  // Καλείται όσο κατεβαίνει η εικόνα (π.χ. ανίχνευση + mqtt.loop()).
  void setServiceCallback(void (*fn)()) { _service = fn; }

  /*  ΔΕΣΜΕΥΣΗ BUFFER - ΚΑΛΕΣΕ ΤΟ ΠΡΙΝ ΤΟ WiFi, όσο το heap είναι ενιαίο.
      Δοκιμάζει πρώτα PSRAM, μετά internal RAM με 90KB περιθώριο ασφαλείας. */
  bool allocBuffer() {
    static const size_t sizes[] = { 200*1024, 128*1024, 96*1024, 72*1024,
                                     56*1024,  40*1024, 28*1024 };
    for (unsigned i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
      uint8_t* p = nullptr;
      if (psramFound()) p = (uint8_t*)ps_malloc(sizes[i]);
      if (!p && sizes[i] <= 96*1024 && ESP.getFreeHeap() > sizes[i] + 90000)
        p = (uint8_t*)malloc(sizes[i]);
      if (p) {
        _buf = p; _cap = sizes[i];
        Serial.printf("[IMG] buffer %u KB σε %s\n",
                      (unsigned)(sizes[i]/1024), psramFound() ? "PSRAM" : "internal RAM");
        return true;
      }
    }
    Serial.println("[IMG] ΔΕΝ βρέθηκε μνήμη για buffer εικόνας -> slideshow OFF");
    return false;
  }

  bool   ready() const { return _buf != nullptr; }
  size_t capacity() const { return _cap; }
  int    count() const { return _count; }

  /* Διαβάζει τη list.txt ΧΩΡΙΣ String. Αν αποτύχει, κρατάει την παλιά λίστα. */
  void fetchList() {
    if (WiFi.status() != WL_CONNECTED) return;

    static char body[1400];
    WiFiClient client;
    HTTPClient http;
    char url[160];
    snprintf(url, sizeof(url), "%s%slist.txt?v=%u", _base, _dir, (uint32_t)esp_random());

    if (!http.begin(client, url)) return;
    http.setConnectTimeout(4000);
    http.setTimeout(5000);
    if (http.GET() != HTTP_CODE_OK) { http.end(); return; }

    int len = http.getSize();
    WiFiClient* s = http.getStreamPtr();
    size_t n = 0;
    unsigned long tEnd = millis() + 5000;
    while (n < sizeof(body) - 1 && millis() < tEnd) {
      if (len > 0 && (int)n >= len) break;
      int avail = s->available();
      if (avail > 0) {
        size_t want = sizeof(body) - 1 - n;
        if ((size_t)avail < want) want = avail;
        int r = s->readBytes(body + n, want);
        if (r > 0) n += r;
      } else if (!s->connected()) {
        break;
      } else {
        delay(2);
      }
    }
    body[n] = '\0';
    http.end();
    if (n == 0) return;                      // αποτυχία -> κρατάμε την παλιά λίστα

    int c = 0;
    char* p = body;
    while (*p && c < MAX_IMAGES) {
      char* e = strchr(p, '\n');
      size_t l = e ? (size_t)(e - p) : strlen(p);
      char name[NAME_LEN];
      trim(name, sizeof(name), p, l);
      if (name[0] != '\0') { strcpy(_names[c], name); c++; }
      if (!e) break;
      p = e + 1;
    }
    if (c > 0) {
      if (c != _count) { _idx = 0; Serial.printf("[IMG] λίστα: %d εικόνες\n", c); }
      _count = c;
    }
  }

  // Κατεβάζει και δείχνει την επόμενη εικόνα. Επιστρέφει false αν απέτυχε.
  bool showNext() {
    if (!_buf || _count <= 0 || WiFi.status() != WL_CONNECTED) return false;
    bool ok = show(_idx);
    _idx = (_idx + 1) % _count;
    return ok;
  }

  bool show(int idx) {
    if (!_buf || idx < 0 || idx >= _count) return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    char url[200];
    snprintf(url, sizeof(url), "%s%s%s?v=%u", _base, _dir, _names[idx], (uint32_t)esp_random());

    WiFiClient client;
    HTTPClient http;
    if (!http.begin(client, url)) return false;
    http.setConnectTimeout(4000);
    http.setTimeout(8000);
    if (http.GET() != HTTP_CODE_OK) { http.end(); return false; }

    int size = http.getSize();
    if (size <= 0 || (size_t)size > _cap) {
      if (size > 0 && !_warned) {
        _warned = true;
        Serial.printf("[IMG] Η εικόνα (%d bytes) δεν χωράει στον buffer (%u bytes).\n"
                      "      Μίκρυνε τις εικόνες ή ενεργοποίησε PSRAM στο board config.\n",
                      size, (unsigned)_cap);
      }
      http.end();
      return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    int got = 0;
    unsigned long timeout = millis() + 8000;
    while (got < size && millis() < timeout) {
      int avail = stream->available();
      if (avail > 0) {
        int want = size - got;
        if (avail > want) avail = want;
        int r = stream->readBytes(_buf + got, avail);
        if (r > 0) got += r;
      } else {
        if (_service) _service();      // η κολόνα αντιδρά όσο κατεβαίνει η εικόνα
        delay(1);
      }
    }
    http.end();
    if (got < size) return false;      // ΟΧΙ free -> ο buffer μένει για την επόμενη

    // Καθάρισμα ΜΟΝΟ αν αλλάζει το μέγεθος: στο 1MHz SPI ένα fillRect όλης της
    // περιοχής κοστίζει ~1 δευτερόλεπτο - το γλιτώνουμε στις περισσότερες αλλαγές.
    uint16_t w = 0, h = 0;
    if (TJpgDec.getJpgSize(&w, &h, _buf, size) == JDR_OK) {
      if (w != _lastW || h != _lastH) {
        _tft->fillRect(_x, _y, _w, _h, TFT_BLACK);
        _lastW = w; _lastH = h;
      }
    } else {
      _tft->fillRect(_x, _y, _w, _h, TFT_BLACK);
      _lastW = _lastH = 0;
    }

    TJpgDec.drawJpg(_x, _y, _buf, size);
    beat();
    return true;
  }

private:
  static bool jpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bmp) {
    if (!s_self || !s_self->_tft) return 0;
    if (y >= s_self->_tft->height()) return 0;
    s_self->_tft->pushImage(x, y, w, h, bmp);
    return 1;
  }

  static void trim(char* dst, size_t dstSize, const char* src, size_t len) {
    while (len > 0 && (*src == ' ' || *src == '\t')) { src++; len--; }
    while (len > 0 && (src[len-1] == ' ' || src[len-1] == '\t' || src[len-1] == '\r')) len--;
    if (len > dstSize - 1) len = dstSize - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
  }

  inline static Slideshow* s_self = nullptr;

  TFT_eSPI*   _tft  = nullptr;
  const char* _base = nullptr;
  const char* _dir  = nullptr;
  int _x = 0, _y = 0, _w = 240, _h = 240;
  uint8_t* _buf = nullptr;
  size_t   _cap = 0;
  char _names[MAX_IMAGES][NAME_LEN] = {};
  int  _count = 0, _idx = 0;
  uint16_t _lastW = 0, _lastH = 0;
  bool _warned = false;
  void (*_service)() = nullptr;
};

} // namespace Kolona
