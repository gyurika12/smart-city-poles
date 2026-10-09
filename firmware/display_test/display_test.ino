/* ============================================================================
   ΤΕΣΤ ΟΘΟΝΗΣ - τίποτα άλλο.
   Ανέβασέ το στην κολόνα 3 και δες ΚΑΙ την οθόνη ΚΑΙ το Serial (115200).

   ΤΙ ΣΗΜΑΙΝΕΙ ΤΟ ΑΠΟΤΕΛΕΣΜΑ:

   A) Βλέπεις ΚΟΚΚΙΝΟ -> ΠΡΑΣΙΝΟ -> ΜΠΛΕ -> κείμενο
      Η οθόνη, η καλωδίωση και το setup είναι ΣΩΣΤΑ.
      Άρα το πρόβλημα είναι στον κώδικα της κολόνας -> πες μου το.

   B) Μένει ΛΕΥΚΗ
      Το tft.init() δεν φτάνει στο πάνελ. Κοίτα τη γραμμή [TFT] στο Serial:
        - Αν λέει setup=User_Setup2 -> μεταγλωττίστηκε με το setup της ΚΟΛΟΝΑΣ 1.
          Λύση: σβήσε το build_opt.h αν βρέθηκε σε λάθος φάκελο + σβήσε τον build.
        - Αν λέει setup=User_Setup με DC=2 RST=4 -> ο κώδικας είναι σωστός,
          άρα φταίει η ΚΑΛΩΔΙΩΣΗ ή ανέβηκε σε ΛΑΘΟΣ ΠΛΑΚΕΤΑ.
          (COL2 και COL3 έχουν την ΙΔΙΑ θύρα σειριακή θύρα στο
           arduino.json - εύκολο να μπερδευτούν.)

   C) Δεν βγαίνει ΤΙΠΟΤΑ στο Serial
      Δεν ανέβηκε το firmware ή λάθος θύρα/ταχύτητα.
   ============================================================================ */

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n===== TEST OTHONIS =====");

  Serial.printf("[TFT] setup=%s\n", USER_SETUP_INFO);
  Serial.printf("[TFT] DC=%d RST=%d MOSI=%d SCLK=%d CS=%d MISO=%d\n",
                TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_CS, TFT_MISO);
  Serial.printf("[TFT] SPI=%d Hz\n", (int)SPI_FREQUENCY);
  Serial.println("[TFT] Αναμενόμενο για COL3: setup=User_Setup  DC=2 RST=4 MISO=13");
  Serial.println("[TFT] Αναμενόμενο για COL1: setup=User_Setup2 DC=7 RST=3 MISO=-1");

  tft.init();
  tft.setRotation(0);

  Serial.printf("[TFT] Διαστάσεις: %d x %d  (σωστό: 240 x 320)\n", tft.width(), tft.height());
  Serial.println("[TFT] Ξεκινάει η αλληλουχία χρωμάτων...");
}

void loop() {
  struct { uint16_t c; const char* n; } colours[] = {
    {TFT_RED, "ΚΟΚΚΙΝΟ"}, {TFT_GREEN, "ΠΡΑΣΙΝΟ"}, {TFT_BLUE, "ΜΠΛΕ"},
  };

  for (auto& col : colours) {
    tft.fillScreen(col.c);
    Serial.printf("  -> %s\n", col.n);
    delay(1200);
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("OTHONI OK", 30, 140);
  tft.setTextSize(1);
  tft.drawString(USER_SETUP_INFO, 30, 175);
  Serial.println("  -> ΚΕΙΜΕΝΟ (αν το βλέπεις, όλα καλά)");
  delay(2500);
}
