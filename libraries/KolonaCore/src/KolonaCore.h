#pragma once
/* ===========================================================================
   KolonaCore - κοινός πυρήνας για τις 3 έξυπνες κολόνες φωτισμού (ESP32-S3)

     KolonaSafety.h     δίχτυ ασφαλείας (watchdog / μνήμη / reboot)
     KolonaDetect.h     ανίχνευση laser + υστέρηση μέρας/νύχτας
     KolonaLed.h        φωτισμός 0/50/78/100% + πρωτόκολλο κύματος
     KolonaNet.h        WiFi + MQTT χωρίς μπλοκάρισμα
     KolonaPublisher.h  MQTT publish μόνο σε αλλαγή τιμής

   Η οθόνη και το slideshow είναι ξεχωριστά στο KolonaDisplay.h, ώστε η COL2
   (που δεν έχει οθόνη) να μην τραβάει τις βιβλιοθήκες TFT.
   =========================================================================== */
#include "KolonaSafety.h"
#include "KolonaDetect.h"
#include "KolonaLed.h"
#include "KolonaNet.h"
#include "KolonaPublisher.h"
