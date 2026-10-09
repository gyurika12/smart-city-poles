#pragma once
/* ---------------------------------------------------------------------------
   ΔΙΧΤΥ ΑΣΦΑΛΕΙΑΣ - κοινό και στις 3 κολόνες
     1) watchdog task: reboot αν το loop() κολλήσει
     2) φρουρός μνήμης: reboot αν πέσει επικίνδυνα το heap
     3) προγραμματισμένο reboot (12h)
     4) αναφορά αιτίας του τελευταίου reboot στο boot
   --------------------------------------------------------------------------- */
#include <Arduino.h>
#include "esp_heap_caps.h"
#include "esp_system.h"

namespace Kolona {

struct SafetyConfig {
  unsigned long rebootAfterMs   = 12UL * 60UL * 60UL * 1000UL;  // 12 ώρες
  size_t        maxBlockMinSafe = 20000;   // bytes - μεγαλύτερο ενιαίο κομμάτι
  size_t        freeHeapMinSafe = 25000;   // bytes - συνολικά ελεύθερα
  unsigned long loopStallMs     = 30000;   // πόσο "σιωπή" του loop = κόλλημα
};

namespace detail {
  inline volatile unsigned long g_beat = 0;
  inline SafetyConfig           g_cfg;

  inline void watchdogTask(void*) {
    for (;;) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      unsigned long b = g_beat;
      if (b != 0 && (millis() - b) > g_cfg.loopStallMs) {
        Serial.println("[SAFETY] loop stalled -> reboot");
        Serial.flush();
        esp_restart();
      }
    }
  }
}

// Σημειώνει "το loop ζει". Καλείται αυτόματα από το safetyLoop(), αλλά και
// χειροκίνητα μέσα σε αργές διαδικασίες (π.χ. κατέβασμα εικόνας).
inline void beat() { detail::g_beat = millis(); }

inline size_t largestFreeBlock() {
  return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

inline void printResetReason() {
  const char* s;
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   s = "POWERON (κανονικό άναμμα)"; break;
    case ESP_RST_SW:        s = "SW (δικό μας ESP.restart)"; break;
    case ESP_RST_PANIC:     s = "PANIC (CRASH! - δες backtrace πιο πάνω)"; break;
    case ESP_RST_INT_WDT:   s = "INT_WDT (κόλλησε σε interrupt)"; break;
    case ESP_RST_TASK_WDT:  s = "TASK_WDT (κόλλησε task)"; break;
    case ESP_RST_WDT:       s = "WDT"; break;
    case ESP_RST_BROWNOUT:  s = "BROWNOUT (ΠΤΩΣΗ ΤΑΣΗΣ - τροφοδοτικό/καλώδιο!)"; break;
    case ESP_RST_DEEPSLEEP: s = "DEEPSLEEP"; break;
    case ESP_RST_EXT:       s = "EXT (reset pin)"; break;
    default:                s = "UNKNOWN"; break;
  }
  Serial.printf("[BOOT] Αιτία τελευταίου reboot: %s\n", s);
}

// Καλείται ΜΙΑ φορά στο τέλος του setup().
inline void safetyBegin(const SafetyConfig& cfg = SafetyConfig()) {
  detail::g_cfg = cfg;
  beat();
  xTaskCreatePinnedToCore(detail::watchdogTask, "wdog", 3072, nullptr, 1, nullptr, 0);
}

// Καλείται στην ΑΡΧΗ κάθε loop().
inline void safetyLoop() {
  unsigned long now = millis();
  beat();

  if (now > detail::g_cfg.rebootAfterMs) {
    Serial.println("[SAFETY] scheduled reboot");
    Serial.flush(); delay(50); ESP.restart();
  }

  if (now > 60000) {
    size_t maxBlk = largestFreeBlock();
    size_t freeH  = ESP.getFreeHeap();
    if (maxBlk < detail::g_cfg.maxBlockMinSafe || freeH < detail::g_cfg.freeHeapMinSafe) {
      Serial.printf("[SAFETY] low memory free=%u maxBlock=%u -> reboot\n",
                    (unsigned)freeH, (unsigned)maxBlk);
      Serial.flush(); delay(50); ESP.restart();
    }
  }
}

} // namespace Kolona
