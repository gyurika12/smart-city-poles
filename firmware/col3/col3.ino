/* ============================================================================
   ΚΟΛΟΝΑ 3  (COL3)  -  ESP32-S3   (με οθόνη ILI9341 + slideshow)
   ----------------------------------------------------------------------------
   Ο κοινός κώδικας (ασφάλεια, φως/κύμα, ανίχνευση, δίκτυο, publish, οθόνη,
   slideshow) είναι στη βιβλιοθήκη KolonaCore. Εδώ μένει ΜΟΝΟ ό,τι αφορά αυτή
   την κολόνα: ρυθμίσεις, pins, topics, οι δικοί της αισθητήρες.

   ΔΟΜΗ:  1. ΡΥΘΜΙΣΕΙΣ  2. PINS  3. ΔΙΚΤΥΟ/TOPICS  4. ΑΝΤΙΚΕΙΜΕΝΑ
          5. ΑΙΣΘΗΤΗΡΕΣ  6. MQTT  7. ΟΘΟΝΗ  8. setup/loop
   ============================================================================ */

// --- Ποιο setup του TFT_eSPI περιμένει αυτή η κολόνα ---------------------
// Η COL3 χρησιμοποιεί το User_Setup.h - την προεπιλογή. Δεν χρειάζεται build_opt.h.
// Αν μεταγλωττιστεί με λάθος setup, η μεταγλώττιση σταματάει με μήνυμα
// (ο έλεγχος είναι στο KolonaDisplay.h) αντί να βγει μαύρη οθόνη.
#define KOLONA_EXPECT_TFT_DC   2
#define KOLONA_EXPECT_TFT_RST  4

#include "secrets.h"      // WiFi / MQTT credentials (not in git) - see secrets.example.h
#include <TFT_eSPI.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <Adafruit_SGP30.h>
#include <Adafruit_VL53L0X.h>
#include <KolonaCore.h>
#include <KolonaDisplay.h>

/* ===========================================================================
   1. ΡΥΘΜΙΣΕΙΣ   <-- ΜΟΝΟ ΕΔΩ ΑΛΛΑΖΕΙΣ
   =========================================================================== */
const int MY_INDEX = 3;                // θέση στον δρόμο (για το κύμα)

// ---- Slideshow (το πιο βαρύ κομμάτι για τη μνήμη) ----
const bool          ENABLE_SLIDESHOW = true;    // false = κολόνα χωρίς εικόνες
const unsigned long IMAGE_CHANGE_MS  = 15000;
const unsigned long LIST_FETCH_MS    = 120000;

// ---- Ανίχνευση (laser TOF) ----
const int TRIGGER_DISTANCE_MM = 200;   // 200 = 20cm
const int NEAR_CONFIRM        = 2;

// ---- Μέρα / Νύχτα (LDR1, pin 6). Υψηλή τιμή = φως ----
const int DARK_ON  = 1500;   // < από αυτό -> ΝΥΧΤΑ (άναψε)
const int DARK_OFF = 2400;   // > από αυτό -> ΜΕΡΑ (σβήσε)

// ---- LDR2 -> ποσοστό φωτεινότητας (ΜΟΝΟ για εμφάνιση) ----
const int LDR_RAW_DARK   = 2000;
const int LDR_RAW_BRIGHT = 4000;

// ---- Χρόνοι φωτισμού ----
const unsigned long MOTION_HOLD_MS = 7000;
const unsigned long WAVE_HOLD_MS   = 2500;

// ---- Ρυθμοί (ms) ----
const unsigned long LOX_INTERVAL_MS  = 50;
const unsigned long LED_UPDATE_MS    = 20;
const unsigned long BME_INTERVAL_MS  = 60000;
const unsigned long SGP_INTERVAL_MS  = 1000;
const unsigned long HCSR_INTERVAL_MS = 300;
const unsigned long LED_PUBLISH_MS   = 500;    // η λάμπα του HA ακολουθεί αυτό
const unsigned long MQTT_SEND_MS     = 5000;
const unsigned long LORA_SEND_MS     = 5000;
const unsigned long BANNER_UPDATE_MS = 100;
const int           BANNER_STEP_PX   = 4;
const unsigned long TOPBAR_UPDATE_MS = 500;
const unsigned long PUB_HEARTBEAT_MS = 10000;

/* ===========================================================================
   2. PINS
   =========================================================================== */
#define LORA_SCK    18
#define LORA_MISO   19
#define LORA_MOSI   17
#define LORA_CS      5
#define LORA_RST    15
#define LORA_DIO0   16

// ΟΘΟΝΗ: η COL3 χρησιμοποιεί το User_Setup.h του TFT_eSPI - την ΠΡΟΕΠΙΛΟΓΗ
// (ILI9341, MISO 13, MOSI 11, SCLK 12, CS 10, DC 2, RST 4, SPI 1 MHz).
// Δεν χρειάζεται build_opt.h εδώ. Όλα τα αναλογικά είναι σε ADC1 (OK με WiFi).

#define LDR_PIN      6      // LDR1 - αποφασίζει μέρα/νύχτα (ADC1, OK με WiFi)
#define LDR2_PIN     1      // LDR2 - μόνο για εμφάνιση
#define LED_PIN     14
#define HCSR04_TRIG  7
#define HCSR04_ECHO  3
#define I2C_SDA      8
#define I2C_SCL      9

/* ===========================================================================
   3. WiFi / MQTT / TOPICS   (τα στοιχεία σύνδεσης είναι στο secrets.h)
   =========================================================================== */
const char* ssid         = WIFI_SSID;
const char* password     = WIFI_PASSWORD;
const char* mqttServer   = MQTT_SERVER;
const int   mqttPort     = 1883;
const char* mqttUser     = MQTT_USER;
const char* mqttPass     = MQTT_PASSWORD;
const char* mqttClientId = "ESP32S3_col3_laserwave";

const char* topicTemp     = "esp32s3/col3/sensors/temperature";
const char* topicHum      = "esp32s3/col3/sensors/humidity";
const char* topicPress    = "esp32s3/col3/sensors/pressure";
const char* topicGas      = "esp32s3/col3/sensors/gas";
const char* topicCO2      = "esp32s3/col3/sensors/eco2";
const char* topicTVOC     = "esp32s3/col3/sensors/tvoc";
const char* topicTOF      = "esp32s3/col3/sensors/distance_tof_mm";
const char* topicUS       = "esp32s3/col3/sensors/distance_us_cm";
const char* topicLDR      = "esp32s3/col3/sensors/ldr";
const char* topicLDR2     = "esp32s3/col3/sensors/ldr2";
const char* topicBright   = "esp32s3/col3/sensors/brightness_pct";
const char* topicLedCmd   = "esp32s3/col3/led/cmd";
const char* topicLedMode  = "esp32s3/col3/led/mode";
const char* topicLedState = "esp32s3/col3/led/state";
const char* topicLedPwm   = "esp32s3/col3/led/pwm";
const char* topicStatus   = "esp32s3/col3/status";
const char* topicWaveTx   = "esp32s3/wave/event";
const char* topicLaser    = "esp32s3/col3/sensors/laser_trigger";

const char* HA_BASE   = HA_BASE_URL;
const char* HA_IMGDIR = "/local/images/";

/* ===========================================================================
   4. ΑΝΤΙΚΕΙΜΕΝΑ & ΚΑΤΑΣΤΑΣΗ
   =========================================================================== */
TFT_eSPI         tft = TFT_eSPI();
Adafruit_BME680  bme;
Adafruit_SGP30   sgp;
Adafruit_VL53L0X lox;

Kolona::Net        net;                 // ΠΡΙΝ τον Publisher (κρατάει reference)
Kolona::Led        led;
Kolona::Detect     detect;
Kolona::Hysteresis night;
Kolona::Banner     banner;
Kolona::Slideshow  show;

enum PubId {
  P_TEMP, P_HUM, P_PRESS, P_GAS, P_CO2, P_TVOC, P_TOF, P_US,
  P_LDR, P_LDR2, P_BRIGHT, P_LASER, P_LEDSTATE, P_LEDMODE, P_LEDPWM, P_COUNT
};
Kolona::Publisher<P_COUNT> pub(net.mqtt());

bool bmeFound = false, sgpFound = false, loxFound = false;
bool hcsr04Found = false, loraOK = false;

int      ldrValue = 0, ldr2Value = 0;
float    temperature = 0, humidity = 0, pressure = 0, gasResistance = 0;
uint16_t distanceMM = 0;
float    distanceUSCM = -1;
uint16_t lastCO2 = 0, lastTVOC = 0;
bool     g_laserTrig = false;
int      packetCounter = 0;

unsigned long lastLOXRead = 0, lastLedUpdate = 0, lastBMERead = 0, lastBmeRetry = 0;
unsigned long lastSGPRead = 0, lastHCSR04Read = 0, lastLedPublish = 0;
unsigned long lastMQTTSend = 0, lastLoRaSend = 0;
unsigned long lastImageChange = 0, lastListFetch = 0;
unsigned long lastBannerUpdate = 0, lastBannerText = 0, lastTopBarUpdate = 0;

volatile bool g_ledStatusDirty = false;   // αίτημα από το mqttCallback

/* ===========================================================================
   5. ΑΙΣΘΗΤΗΡΕΣ
   =========================================================================== */
float readHCSR04cm() {
  digitalWrite(HCSR04_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(HCSR04_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(HCSR04_TRIG, LOW);
  unsigned long d = pulseIn(HCSR04_ECHO, HIGH, 25000);
  if (d == 0) return -1;
  return d * 0.0343f / 2.0f;
}

void bmeApplySettings() {
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);
}

int ldrToPct(int raw) {
  return map(constrain(raw, LDR_RAW_DARK, LDR_RAW_BRIGHT), LDR_RAW_DARK, LDR_RAW_BRIGHT, 0, 100);
}

// Laser κάθε 50ms + ράμπα φωτός σε σταθερό ρυθμό 20ms.
// Καλείται από το loop ΚΑΙ μέσα από το κατέβασμα εικόνας, ώστε η κολόνα να
// αντιδρά ακόμη κι όσο φορτώνει εικόνα.
void serviceDetection() {
  unsigned long now = millis();
  Kolona::beat();

  if (loxFound && now - lastLOXRead >= LOX_INTERVAL_MS) {
    lastLOXRead = now;
    VL53L0X_RangingMeasurementData_t m;
    lox.rangingTest(&m, false);
    distanceMM = (m.RangeStatus == 0) ? m.RangeMilliMeter : 0;   // μόνο καθαρές μετρήσεις
    g_laserTrig = detect.update(distanceMM > 0 ? (int)distanceMM : -1);
  }
  if (now - lastLedUpdate >= LED_UPDATE_MS) {
    lastLedUpdate = now;
    led.update(night.update(ldrValue), g_laserTrig);
  }
}

void serviceSensors() {
  unsigned long now = millis();

  ldrValue  = analogRead(LDR_PIN);
  ldr2Value = analogRead(LDR2_PIN);

  // Αν ο BME χαθεί, ξαναδοκίμασε κάθε 10s - ΧΩΡΙΣ reboot, χωρίς μπλοκάρισμα
  if (!bmeFound && now - lastBmeRetry > 10000) {
    lastBmeRetry = now;
    if (bme.begin(0x76, &Wire) || bme.begin(0x77, &Wire)) {
      bmeFound = true; bmeApplySettings();
      Serial.println("[BME] recovered");
    }
  }
  if (bmeFound && now - lastBMERead > BME_INTERVAL_MS) {
    lastBMERead = now;
    if (bme.performReading()) {
      temperature   = bme.temperature;
      humidity      = bme.humidity;
      pressure      = bme.pressure / 100.0f;
      gasResistance = bme.gas_resistance / 1000.0f;
    } else {
      bmeFound = false;   // θα ξαναδοκιμάσει μόνο του
    }
  }

  if (sgpFound && now - lastSGPRead > SGP_INTERVAL_MS) {
    lastSGPRead = now;
    if (sgp.IAQmeasure()) { lastCO2 = sgp.eCO2; lastTVOC = sgp.TVOC; }
  }

  if (hcsr04Found && now - lastHCSR04Read > HCSR_INTERVAL_MS) {
    lastHCSR04Read = now;
    float d = readHCSR04cm();
    if (d > 0) distanceUSCM = d;
  }
}

/* ===========================================================================
   6. MQTT
   =========================================================================== */
void publishLedStatus() {
  if (!net.mqttOk()) return;
  pub.s(P_LEDMODE,  topicLedMode,  led.modeString(), (float)led.mode());
  pub.t(P_LEDSTATE, topicLedState, led.on(), "ON", "OFF");
  pub.i(P_LEDPWM,   topicLedPwm,   led.pwm(), 3);
}

void sendWave() {
  if (net.mqttOk()) net.mqtt().publish(topicWaveTx, "col3", false);
}

void onMqttConnect() {
  net.mqtt().subscribe(topicLedCmd);
  net.mqtt().subscribe(topicWaveTx);
  pub.forceAll();                 // ξαναστείλε τα πάντα μετά την επανασύνδεση
  publishLedStatus();
}

// ΠΡΟΣΟΧΗ: εδώ ΔΕΝ κάνουμε publish. Σηκώνουμε σημαία και το loop το στέλνει.
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  char msg[24];
  Kolona::normalizePayload(msg, sizeof(msg), payload, length);

  if (!strcmp(topic, topicLedCmd)) {
    led.applyCommand(msg);
    g_ledStatusDirty = true;
    return;
  }
  if (!strcmp(topic, topicWaveTx)) {
    if (Kolona::waveIsFromNeighbour(msg, MY_INDEX)) {
      led.onNeighbourWave();
      Serial.printf("WAVE recv from %s\n", msg);
    }
  }
}

void publishSensors() {
  if (!net.mqttOk()) return;
  pub.f(P_TEMP,   topicTemp,   temperature,   1, 0.1f);
  pub.f(P_HUM,    topicHum,    humidity,      1, 0.3f);
  pub.f(P_PRESS,  topicPress,  pressure,      1, 0.2f);
  pub.f(P_GAS,    topicGas,    gasResistance, 1, 0.5f);
  pub.i(P_CO2,    topicCO2,    lastCO2,   5);
  pub.i(P_TVOC,   topicTVOC,   lastTVOC,  2);
  pub.i(P_TOF,    topicTOF,    distanceMM, 5);
  pub.f(P_US,     topicUS,     distanceUSCM, 1, 1.0f);
  pub.i(P_LDR,    topicLDR,    ldrValue,  20);
  pub.i(P_LDR2,   topicLDR2,   ldr2Value, 20);
  pub.i(P_BRIGHT, topicBright, ldrToPct(ldr2Value), 1);
  pub.t(P_LASER,  topicLaser,  g_laserTrig, "ON", "OFF");
}

// Καλείται όσο κατεβαίνει εικόνα: η κολόνα συνεχίζει να ανιχνεύει και το MQTT
// μένει ζωντανό.
void slideshowService() {
  serviceDetection();
  if (net.mqttOk()) net.mqtt().loop();
}

/* ===========================================================================
   7. ΟΘΟΝΗ
   =========================================================================== */
void drawTopBar() {
  char b[16];
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);

  snprintf(b, sizeof(b), "TX%d", packetCounter);
  Kolona::topBarField(tft, 5, 4, 38, b, TFT_WHITE);
  snprintf(b, sizeof(b), "%.1fC", temperature);
  Kolona::topBarField(tft, 45, 4, 45, b, TFT_WHITE);
  snprintf(b, sizeof(b), "%umm", distanceMM);
  Kolona::topBarField(tft, 92, 4, 51, b, TFT_MAGENTA);

  if (distanceUSCM > 0) snprintf(b, sizeof(b), "%.0f", distanceUSCM);
  else                  strcpy(b, "---");
  Kolona::topBarField(tft, 145, 4, 18, b, TFT_ORANGE);

  Kolona::topBarField(tft, 165, 4, 38, led.modeString(), led.on() ? TFT_GREEN : TFT_RED);
  Kolona::topBarField(tft, 205, 4, 33, "WiFi", net.wifiOk() ? TFT_CYAN : TFT_RED);

  tft.setTextPadding(0);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

void updateBannerText() {
  char t[96];
  snprintf(t, sizeof(t), "LDR1:%d LDR2:%d TOF:%u B:%d TR:%d PWM:%d",
           ldrValue, ldr2Value, distanceMM, detect.lastValidMm(),
           g_laserTrig ? 1 : 0, led.pwm());
  banner.setText(t);
}

/* ===========================================================================
   8. setup() / loop()
   =========================================================================== */
void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n===== COL3 START =====");
  Kolona::printResetReason();

  pinMode(LDR_PIN, INPUT);
  pinMode(LDR2_PIN, INPUT);
  pinMode(HCSR04_TRIG, OUTPUT);
  pinMode(HCSR04_ECHO, INPUT);
  digitalWrite(HCSR04_TRIG, LOW);
  analogReadResolution(12);

  // --- Φως / ανίχνευση ---
  led.begin(LED_PIN);
  led.setHold(MOTION_HOLD_MS, WAVE_HOLD_MS);
  led.setWaveSender(sendWave);
  detect.configure(TRIGGER_DISTANCE_MM, NEAR_CONFIRM);
  night.configure(DARK_ON, DARK_OFF);

  // --- I2C + αισθητήρες ---
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setTimeOut(50);              // δεν κολλάει για πάντα αν γλιστρήσει ο bus
  delay(100);

  if (bme.begin(0x76, &Wire) || bme.begin(0x77, &Wire)) {
    bmeFound = true; bmeApplySettings(); Serial.println("BME680 OK");
  } else Serial.println("BME680 FAIL");

  if (sgp.begin(&Wire)) { sgpFound = true; sgp.IAQinit(); Serial.println("SGP30 OK"); }
  else                    Serial.println("SGP30 FAIL");

  for (int i = 0; i < 3 && !hcsr04Found; i++) {
    float d = readHCSR04cm();
    if (d >= 1 && d <= 400) { hcsr04Found = true; distanceUSCM = d; }
    else delay(60);
  }
  Serial.println(hcsr04Found ? "HC-SR04 OK" : "HC-SR04 FAIL");

  if (lox.begin(0x29, false, &Wire)) { loxFound = true; Serial.println("VL53L0X OK"); }
  else                                 Serial.println("VL53L0X FAIL");

  // --- Οθόνη ---
  tft.init();
  // Τυπώνει ΠΟΙΟ setup του TFT_eSPI είναι όντως ενεργό. Αν δεις λάθος τιμές εδώ,
  // η οθόνη θα μείνει μαύρη -> δες το ΔΙΑΒΑΣΕ_ΜΕ.md (ενότητα «οθόνη»).
  Serial.printf("[TFT] setup=%s  DC=%d RST=%d MOSI=%d SCLK=%d CS=%d MISO=%d  SPI=%d Hz\n",
                USER_SETUP_INFO, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_CS, TFT_MISO,
                (int)SPI_FREQUENCY);
  tft.setRotation(0);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawFastHLine(0, 295, 240, TFT_CYAN);   // γραμμή banner: μία φορά

  banner.begin(tft, 296);
  banner.setText("COL3 READY");

  // --- Slideshow: ο buffer ΠΡΙΝ το WiFi, όσο το heap είναι ενιαίο ---
  if (ENABLE_SLIDESHOW) {
    show.begin(tft, HA_BASE, HA_IMGDIR, 0, 16, 240, 279);
    show.setServiceCallback(slideshowService);
    show.allocBuffer();
  }

  // --- LoRa (SPI2/FSPI - η οθόνη είναι σε HSPI, δεν συγκρούονται) ---
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  if (LoRa.begin(433E6)) {
    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setSyncWord(0x12);
    loraOK = true; Serial.println("LoRa OK");
  } else Serial.println("LoRa FAIL");

  // --- Δίκτυο ---
  pub.setHeartbeat(PUB_HEARTBEAT_MS);
  net.begin(ssid, password, mqttServer, mqttPort,
            mqttUser, mqttPass, mqttClientId, topicStatus, 1);
  net.setCallback(mqttCallback);
  net.setOnConnect(onMqttConnect);
  net.setMqttRetryMs(5000);
  net.connectWifiBlocking();
  net.reconnect();

  ldrValue  = analogRead(LDR_PIN);
  ldr2Value = analogRead(LDR2_PIN);

  if (ENABLE_SLIDESHOW && show.ready() && net.wifiOk()) {
    show.fetchList();
    lastListFetch = millis();
    if (show.count() > 0) { show.showNext(); lastImageChange = millis(); }
    else                  tft.drawString("NO IMAGES", 20, 100);
  }

  Serial.printf("[HEAP] μετά το setup: free=%u maxBlock=%u\n",
                ESP.getFreeHeap(), Kolona::largestFreeBlock());

  Kolona::safetyBegin();
}

void loop() {
  unsigned long now = millis();

  Kolona::safetyLoop();     // 1. δίχτυ ασφαλείας
  net.loop();               // 2. WiFi + MQTT χωρίς μπλοκάρισμα
  serviceDetection();       // 3. laser + ράμπα φωτός
  serviceSensors();         // 4. LDR / BME / SGP30 / HC-SR04

  if (g_ledStatusDirty && net.mqttOk()) { g_ledStatusDirty = false; publishLedStatus(); }

  // 5. Slideshow
  if (ENABLE_SLIDESHOW && show.ready()) {
    if (net.wifiOk() && now - lastListFetch > LIST_FETCH_MS) {
      lastListFetch = now;
      show.fetchList();
    }
    if (show.count() > 0 && now - lastImageChange > IMAGE_CHANGE_MS) {
      lastImageChange = now;
      show.showNext();
    }
  }

  // 6. Οθόνη
  if (now - lastBannerText > 500)                { lastBannerText = now; updateBannerText(); }
  if (now - lastBannerUpdate > BANNER_UPDATE_MS) { lastBannerUpdate = now; banner.tick(BANNER_STEP_PX); }
  if (now - lastTopBarUpdate > TOPBAR_UPDATE_MS) { lastTopBarUpdate = now; drawTopBar(); }

  // 7. LoRa
  if (loraOK && now - lastLoRaSend > LORA_SEND_MS) {
    lastLoRaSend = now;
    char packet[210];
    snprintf(packet, sizeof(packet),
             "COL3,TEMP:%.1f,HUM:%.1f,PRESS:%.1f,GAS:%.1f,CO2:%u,TVOC:%u,DIST:%u,US:%.1f,"
             "LDR1:%d,LDR2:%d,LED:%s,PWM:%d,TRIG:%d",
             temperature, humidity, pressure, gasResistance,
             lastCO2, lastTVOC, distanceMM, distanceUSCM,
             ldrValue, ldr2Value, led.on() ? "ON" : "OFF", led.pwm(), g_laserTrig ? 1 : 0);
    LoRa.beginPacket();
    LoRa.print(packet);
    LoRa.endPacket();
    packetCounter++;
  }

  // 8. MQTT: κατάσταση LED γρήγορα (η λάμπα του HA την ακολουθεί),
  //          δεδομένα αισθητήρων αργά.
  if (now - lastLedPublish > LED_PUBLISH_MS) { lastLedPublish = now; publishLedStatus(); }
  if (now - lastMQTTSend   > MQTT_SEND_MS)   { lastMQTTSend   = now; publishSensors(); }

  // 9. Debug (1/δευτ.)
  static unsigned long lastDbg = 0;
  if (now - lastDbg > 1000) {
    lastDbg = now;
    Serial.printf("LDR1=%d LDR2=%d DARK=%d MODE=%s TOF=%u TRIG=%d PWM=%d | free=%u maxBlock=%u\n",
                  ldrValue, ldr2Value, night.value() ? 1 : 0, led.modeString(),
                  distanceMM, g_laserTrig ? 1 : 0, led.pwm(),
                  ESP.getFreeHeap(), Kolona::largestFreeBlock());
  }
}
