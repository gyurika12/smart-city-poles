// ============================================================================
//  secrets.example.h  —  template for network credentials
//  Πρότυπο για τα στοιχεία σύνδεσης
// ----------------------------------------------------------------------------
//  EN: Copy this file as  secrets.h  into each sketch folder
//      (col1/, col2/, col3/, camera/) and fill in your own values.
//      secrets.h is listed in .gitignore and is never committed.
//
//  ΕΛ: Αντίγραψε αυτό το αρχείο ως  secrets.h  μέσα σε κάθε φάκελο sketch
//      (col1/, col2/, col3/, camera/) και συμπλήρωσε τις δικές σου τιμές.
//      Το secrets.h είναι στο .gitignore και δεν ανεβαίνει ποτέ στο git.
// ============================================================================
#pragma once

#define WIFI_SSID      "your-wifi-ssid"
#define WIFI_PASSWORD  "your-wifi-password"

#define MQTT_SERVER    "your-broker-ip"        // IP of the Mosquitto broker / Home Assistant
#define MQTT_USER      "your-mqtt-user"
#define MQTT_PASSWORD  "your-mqtt-password"

#define HA_BASE_URL    "http://your-ha-ip:8123"     // Home Assistant (slideshow images, COL1/COL3)
