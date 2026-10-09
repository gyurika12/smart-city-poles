/* ============================================================================
   ΚΟΛΟΝΑ 1  (COL1)  -  ESP32-S3   (με οθόνη ILI9341 + slideshow)
   ----------------------------------------------------------------------------
   Ο κοινός κώδικας (ασφάλεια, φως/κύμα, ανίχνευση, δίκτυο, publish, οθόνη,
   slideshow) είναι στη βιβλιοθήκη KolonaCore. Εδώ μένει ΜΟΝΟ ό,τι αφορά αυτή
   την κολόνα: ρυθμίσεις, pins, topics, οι δικοί της αισθητήρες.

   ΔΟΜΗ:  1. ΡΥΘΜΙΣΕΙΣ  2. PINS  3. ΔΙΚΤΥΟ/TOPICS  4. ΑΝΤΙΚΕΙΜΕΝΑ
          5. ΑΙΣΘΗΤΗΡΕΣ  6. MQTT  7. ΟΘΟΝΗ  8. setup/loop
   ============================================================================ */

#include "secrets.h"      // WiFi / MQTT credentials (not in git) - see secrets.example.h
#include <Wire.h>
#include <BH1750.h>
#include "DFRobot_LTR390UV.h"
#include "DFRobot_BME68x.h"
// --- Ποιο setup του TFT_eSPI περιμένει αυτή η κολόνα ---------------------
// Η COL1 χρειάζεται το User_Setup2.h -> το ορίζει το build_opt.h αυτού του φακέλου.
// ΜΗΝ ΣΒΗΣΕΙΣ ΤΟ build_opt.h.
// Αν μεταγλωττιστεί με λάθος setup, η μεταγλώττιση σταματάει με μήνυμα
// (ο έλεγχος είναι στο KolonaDisplay.h) αντί να βγει μαύρη οθόνη.
#define KOLONA_EXPECT_TFT_DC   7
#define KOLONA_EXPECT_TFT_RST  3

#include <TFT_eSPI.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_VL53L0X.h>
#include <KolonaCore.h>
#include <KolonaDisplay.h>

/* ===========================================================================
   1. ΡΥΘΜΙΣΕΙΣ   <-- ΜΟΝΟ ΕΔΩ ΑΛΛΑΖΕΙΣ
   =========================================================================== */
const int MY_INDEX = 1;                // θέση στον δρόμο (για το κύμα)

// ---- Slideshow (το πιο βαρύ κομμάτι για τη μνήμη) ----
const bool          ENABLE_SLIDESHOW = true;    // false = κολόνα χωρίς εικόνες
const unsigned long IMAGE_CHANGE_MS  = 15000;
const unsigned long LIST_FETCH_MS    = 60000;

// ---- PIR ----
// Στη μακέτα έβγαζε συνεχώς ψευδείς ανιχνεύσεις, γι' αυτό ο παλιός κώδικας
// έστελνε μόνιμα "OFF". Βάλε true για την πραγματική τιμή.
const bool PIR_ENABLED = true;

// ---- Ανίχνευση (laser TOF) ----
const int TRIGGER_DISTANCE_MM = 200;   // 200 = 20cm
const int NEAR_CONFIRM        = 2;

// ---- Μέρα / Νύχτα (BH1750, lux) ----
// ΒΑΘΜΟΝΟΜΗΜΕΝΟ ΜΕ ΠΡΑΓΜΑΤΙΚΕΣ ΜΕΤΡΗΣΕΙΣ (14/09/2026, ιστορικό Home Assistant):
//    φως αναμμένο -> ~445 lux
//    φως σβηστό   -> ~12,5-19,2 lux
// Το παλιό κατώφλι 15.0 έπεφτε ΜΕΣΑ στο εύρος του σκοταδιού (12,5-19,2):
// η κολόνα δεν ήξερε αν είναι μέρα ή νύχτα και τρεμόπαιζε.
const float LIGHT_DARK_ON  = 40.0f;    // < από αυτό lux -> ΝΥΧΤΑ (άναψε)
const float LIGHT_DARK_OFF = 100.0f;   // > από αυτό lux -> ΜΕΡΑ (σβήσε). 40..100 = νεκρή ζώνη

// ---- Χρόνοι φωτισμού ----
const unsigned long MOTION_HOLD_MS = 7000;
const unsigned long WAVE_HOLD_MS   = 2500;

// ---- Ρυθμοί (ms) ----
const unsigned long SENSOR_INTERVAL_MS = 500;
const unsigned long LOX_INTERVAL_MS    = 50;    // ανάγνωση laser
const unsigned long LED_UPDATE_MS      = 20;    // ράμπα φωτός (ίδια με COL2/COL3)
const unsigned long BME_PERIOD_MS      = 3000;  // BME688 χωρίς delay()
const unsigned long BANNER_UPDATE_MS   = 100;
const int           BANNER_STEP_PX     = 4;
const unsigned long TOPBAR_UPDATE_MS   = 500;
const unsigned long PUB_HEARTBEAT_MS   = 10000;

/* ===========================================================================
   2. PINS
   ---------------------------------------------------------------------------
   ΟΘΟΝΗ: η COL1 χρησιμοποιεί το User_Setup2.h του TFT_eSPI
   (ILI9341_2, MOSI 11, SCLK 12, CS 10, DC 7, RST 3, MISO αχρησιμοποίητο, 20 MHz).
   Η επιλογή γίνεται αυτόματα από το build_opt.h αυτού του φακέλου, που ορίζει
   το -DKOLONA_TFT_SETUP2.  ΜΗΝ ΣΒΗΣΕΙΣ ΤΟ build_opt.h: χωρίς αυτό η οθόνη
   παίρνει το setup της COL3 (DC 2, RST 4) και μένει μαύρη.
   Καμία σύγκρουση ακροδεκτών με την οθόνη.

   !! ΠΡΟΣΟΧΗ (δεν το αλλάζω, αλλά να το ξέρεις) !!
   LDR_PIN 13 και AIR_QUAL_PIN 14 είναι σε ADC2. Στο ESP32 το ADC2 το
   χρησιμοποιεί το WiFi, οπότε με ενεργή ασύρματη σύνδεση οι αναγνώσεις τους
   δεν είναι αξιόπιστες (συνήθως 0). Αν οι τιμές ldr/air_quality βγαίνουν 0 ή
   τυχαίες, αυτή είναι η αιτία: μετακίνησέ τα σε ADC1 (GPIO 1..10).
   Το GAS_PIN 4 είναι σε ADC1 και δεν έχει πρόβλημα.
   =========================================================================== */
#define PIR_PIN       17
#define TRIG_PIN      15
#define ECHO_PIN      16
#define LED_PIN       19
#define RAIN_PIN      18
#define AIR_QUAL_PIN  14
#define GAS_PIN        4
#define LDR_PIN       13
#define I2C_SDA        8
#define I2C_SCL        9

#define LORA_SCK      36
#define LORA_MISO     37
#define LORA_MOSI     35
#define LORA_NSS      39
#define LORA_RST       5
#define LORA_DIO0      6
#define LORA_FREQ      433E6

/* ===========================================================================
   3. WiFi / MQTT / TOPICS   (τα στοιχεία σύνδεσης είναι στο secrets.h)
   =========================================================================== */
const char* ssid           = WIFI_SSID;
const char* password       = WIFI_PASSWORD;
const char* mqtt_server    = MQTT_SERVER;
const int   mqtt_port      = 1883;
const char* mqtt_user      = MQTT_USER;
const char* mqtt_pass      = MQTT_PASSWORD;
const char* mqtt_client_id = "ESP32S3_col1_final";

const char* topicTemp      = "esp32s3/col1/sensors/temperature";
const char* topicHum       = "esp32s3/col1/sensors/humidity";
const char* topicPress     = "esp32s3/col1/sensors/pressure";
const char* topicGasBME    = "esp32s3/col1/sensors/gas_bme";
const char* topicGasAn     = "esp32s3/col1/sensors/gas_analog";
const char* topicLux       = "esp32s3/col1/sensors/lux";
const char* topicUV        = "esp32s3/col1/sensors/uv";
const char* topicUS        = "esp32s3/col1/sensors/distance_us_cm";
const char* topicTOF       = "esp32s3/col1/sensors/distance_tof_mm";
const char* topicPIR       = "esp32s3/col1/sensors/motion";
const char* topicRain      = "esp32s3/col1/sensors/rain";
const char* topicAirQ      = "esp32s3/col1/sensors/air_quality";
const char* topicLDR       = "esp32s3/col1/sensors/ldr";
const char* topicLaserTrig = "esp32s3/col1/sensors/laser_trigger";
const char* topicLedCmd    = "esp32s3/col1/led/cmd";
const char* topicLedState  = "esp32s3/col1/led/state";
const char* topicLedMode   = "esp32s3/col1/led/mode";
const char* topicLedPwm    = "esp32s3/col1/led/pwm";
const char* topicWaveTx    = "esp32s3/wave/event";
const char* topicStatus    = "esp32s3/col1/status";

const char* HA_BASE    = HA_BASE_URL;
const char* HA_IMG_DIR = "/local/images/";

/* ===========================================================================
   4. ΑΝΤΙΚΕΙΜΕΝΑ & ΚΑΤΑΣΤΑΣΗ
   =========================================================================== */
SPIClass           loraSPI(FSPI);     // ξεχωριστός SPI για LoRa (οθόνη = HSPI)
BH1750             lightMeter;
DFRobot_LTR390UV   ltr390(LTR390UV_DEVICE_ADDR, &Wire);
DFRobot_BME68x_I2C bme(0x77);
Adafruit_VL53L0X   lox;
TFT_eSPI           tft = TFT_eSPI();

Kolona::Net        net;                 // ΠΡΙΝ τον Publisher (κρατάει reference)
Kolona::Led        led;
Kolona::Detect     detect;
Kolona::Hysteresis night;
Kolona::Banner     banner;
Kolona::Slideshow  show;

enum PubId {
  P_TEMP, P_HUM, P_PRESS, P_GASBME, P_LUX, P_UV, P_US, P_TOF,
  P_PIR, P_RAIN, P_AIRQ, P_GASAN, P_LDR, P_LASER,
  P_LEDSTATE, P_LEDMODE, P_LEDPWM, P_COUNT
};
Kolona::Publisher<P_COUNT> pub(net.mqtt());

bool bh1750Found = false, ltr390Found = false, bme688Found = false;
bool tofFound = false, loraFound = false;

// --- BME688 χωρίς delay ---
float bmeTemp = 0, bmeHum = 0, bmePres = 0, bmeGasVal = 0;
unsigned long lastBmeConvert = 0;
bool bmeConverting = false;

// --- τελευταίες τιμές ---
float disp_lux = -1, disp_uv = 0, disp_us = -1;
int   disp_tof = -1;
bool  disp_laserTrig = false, disp_pir = false;
int   rainVal = 1, airQualVal = 0, gasVal = 0, ldrVal = 0;
int   packetCounter = 0;

unsigned long previousMillis = 0, lastLOXRead = 0, lastLedUpdate = 0;
unsigned long lastImageChange = 0, lastListFetch = 0;
unsigned long lastBannerUpdate = 0, lastBannerText = 0, lastTopBarUpdate = 0;

volatile bool g_ledStatusDirty = false;   // αίτημα από το mqttCallback

/* ===========================================================================
   5. ΑΙΣΘΗΤΗΡΕΣ
   =========================================================================== */
float readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long d = pulseIn(ECHO_PIN, HIGH, 25000);
  if (d == 0) return -1;
  return (d * 0.0343f) / 2.0f;
}

// Διαβάζει το BME688 ΧΩΡΙΣ delay (πριν: delay(300) κάθε 500ms = πάγωνε το loop)
void serviceBME() {
  if (!bme688Found) return;
  unsigned long now = millis();
  if (!bmeConverting && now - lastBmeConvert >= BME_PERIOD_MS) {
    bme.startConvert();
    lastBmeConvert = now;
    bmeConverting = true;
  } else if (bmeConverting && now - lastBmeConvert >= 350) {
    bme.update();
    bmeTemp   = bme.readTemperature() / 100.0f;
    bmeHum    = bme.readHumidity() / 1000.0f;
    bmePres   = bme.readPressure();
    bmeGasVal = bme.readGasResistance();
    bmeConverting = false;
  }
}

// Laser κάθε 50ms + ράμπα φωτός σε σταθερό ρυθμό 20ms.
// Καλείται από το loop ΚΑΙ μέσα από το κατέβασμα εικόνας.
void serviceDetection() {
  unsigned long now = millis();
  Kolona::beat();

  if (tofFound && now - lastLOXRead >= LOX_INTERVAL_MS) {
    lastLOXRead = now;
    VL53L0X_RangingMeasurementData_t m;
    lox.rangingTest(&m, false);
    disp_tof = (m.RangeStatus == 0) ? m.RangeMilliMeter : -1;
    disp_laserTrig = detect.update(disp_tof);
  }
  if (now - lastLedUpdate >= LED_UPDATE_MS) {
    lastLedUpdate = now;
    // Χωρίς έγκυρη μέτρηση lux -> κρατάει την προηγούμενη απόφαση μέρας/νύχτας
    bool dark = night.updateIfValid(disp_lux, bh1750Found && disp_lux >= 0);
    led.update(dark, disp_laserTrig);
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
  if (net.mqttOk()) net.mqtt().publish(topicWaveTx, "col1", false);
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
    led.applyCommand(msg, true);    // ό,τι δεν αναγνωρίζεται -> AUTO (όπως πριν)
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
  pub.f(P_TEMP,   topicTemp,      bmeTemp,   1, 0.1f);
  pub.f(P_HUM,    topicHum,       bmeHum,    1, 0.3f);
  pub.f(P_PRESS,  topicPress,     bmePres,   0, 10.0f);
  pub.f(P_GASBME, topicGasBME,    bmeGasVal, 0, 500.0f);
  pub.f(P_LUX,    topicLux,       disp_lux,  1, 1.0f);
  pub.f(P_UV,     topicUV,        disp_uv,   2, 0.02f);
  pub.f(P_US,     topicUS,        disp_us,   1, 1.0f);
  pub.i(P_TOF,    topicTOF,       disp_tof,     5);
  pub.t(P_PIR,    topicPIR,       PIR_ENABLED ? disp_pir : false, "ON", "OFF");
  pub.t(P_RAIN,   topicRain,      rainVal == 0, "RAIN", "DRY");
  pub.i(P_AIRQ,   topicAirQ,      airQualVal,  20);
  pub.i(P_GASAN,  topicGasAn,     gasVal,      20);
  pub.i(P_LDR,    topicLDR,       ldrVal,      20);
  pub.t(P_LASER,  topicLaserTrig, disp_laserTrig, "ON", "OFF");
  publishLedStatus();
}

void sendLoRa() {
  if (!loraFound) return;
  char packet[210];
  snprintf(packet, sizeof(packet),
           "COL:1,LUX:%.1f,TOF:%d,TRIG:%d,PWM:%d,TEMP:%.1f,HUM:%.1f,PRES:%.0f,BGAS:%.0f,"
           "US:%.1f,RAIN:%d,AIR:%d,GAS:%d,LDR:%d",
           disp_lux, disp_tof, disp_laserTrig ? 1 : 0, led.pwm(),
           bmeTemp, bmeHum, bmePres, bmeGasVal,
           disp_us, rainVal, airQualVal, gasVal, ldrVal);
  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();
  packetCounter++;
}

// Καλείται όσο κατεβαίνει εικόνα.
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

  snprintf(b, sizeof(b), "TX:%d", packetCounter);
  Kolona::topBarField(tft, 5, 4, 38, b, TFT_WHITE);
  snprintf(b, sizeof(b), "%.1fC", bmeTemp);
  Kolona::topBarField(tft, 45, 4, 45, b, TFT_WHITE);

  if (disp_lux >= 0) snprintf(b, sizeof(b), "%.0flx", disp_lux);
  else               strcpy(b, "---lx");
  Kolona::topBarField(tft, 92, 4, 51, b, TFT_MAGENTA);

  if (disp_tof >= 0) snprintf(b, sizeof(b), "%d", disp_tof);
  else               strcpy(b, "---");
  Kolona::topBarField(tft, 145, 4, 31, b, TFT_ORANGE);

  Kolona::topBarField(tft, 178, 4, 25, led.on() ? "LED" : "---", led.on() ? TFT_GREEN : TFT_RED);
  Kolona::topBarField(tft, 205, 4, 33, "WiFi", net.wifiOk() ? TFT_CYAN : TFT_RED);

  tft.setTextPadding(0);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

void updateBannerText() {
  char t[96];
  snprintf(t, sizeof(t), "LUX:%.1f TOF:%d BASE:%d TRIG:%d PWM:%d",
           disp_lux, disp_tof, detect.lastValidMm(), disp_laserTrig ? 1 : 0, led.pwm());
  banner.setText(t);
}

/* ===========================================================================
   8. setup() / loop()
   =========================================================================== */
void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n===== COL1 START =====");
  Kolona::printResetReason();

  pinMode(PIR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RAIN_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  analogReadResolution(12);

  // --- Φως / ανίχνευση ---
  led.begin(LED_PIN);
  led.setHold(MOTION_HOLD_MS, WAVE_HOLD_MS);
  led.setWaveSender(sendWave);
  detect.configure(TRIGGER_DISTANCE_MM, NEAR_CONFIRM);
  night.configure(LIGHT_DARK_ON, LIGHT_DARK_OFF);

  // --- I2C + αισθητήρες ---
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setTimeOut(50);              // δεν κολλάει για πάντα αν γλιστρήσει ο bus

  bh1750Found = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  Serial.println(bh1750Found ? "BH1750 OK" : "BH1750 FAIL");

  if (ltr390.begin() == 0) {
    ltr390Found = true;
    ltr390.setALSOrUVSMeasRate(ltr390.e18bit, ltr390.e100ms);
    ltr390.setALSOrUVSGain(ltr390.eGain3);
    ltr390.setMode(ltr390.eUVSMode);
    Serial.println("LTR390 OK");
  } else Serial.println("LTR390 FAIL");

  if (bme.begin() == 0) {
    bme688Found = true;
    bme.setGasHeater(320, 150);
    Serial.println("BME688 OK");
  } else Serial.println("BME688 FAIL");

  tofFound = lox.begin();
  Serial.println(tofFound ? "VL53L0X OK" : "VL53L0X FAIL");

  // --- Οθόνη ---
  tft.init();
  // Τυπώνει ΠΟΙΟ setup του TFT_eSPI είναι όντως ενεργό. Αν δεις λάθος τιμές εδώ,
  // η οθόνη θα μείνει μαύρη -> δες το ΔΙΑΒΑΣΕ_ΜΕ.md (ενότητα «οθόνη»).
  Serial.printf("[TFT] setup=%s  DC=%d RST=%d MOSI=%d SCLK=%d CS=%d MISO=%d  SPI=%d Hz\n",
                USER_SETUP_INFO, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_CS, TFT_MISO,
                (int)SPI_FREQUENCY);
  tft.writecommand(0x21);          // invert on
  tft.setRotation(2);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawFastHLine(0, 295, 240, TFT_CYAN);   // γραμμή banner: μία φορά

  banner.begin(tft, 296);
  banner.setText("COL1 READY");

  // --- Slideshow: ο buffer ΠΡΙΝ το WiFi, όσο το heap είναι ενιαίο ---
  if (ENABLE_SLIDESHOW) {
    show.begin(tft, HA_BASE, HA_IMG_DIR, 0, 16, 240, 279);
    show.setServiceCallback(slideshowService);
    show.allocBuffer();
  }

  // --- LoRa σε ξεχωριστό SPI (FSPI) ---
  loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  LoRa.setSPI(loraSPI);
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  loraFound = LoRa.begin(LORA_FREQ);
  Serial.println(loraFound ? "LoRa OK" : "LoRa FAIL");

  // --- Δίκτυο ---
  pub.setHeartbeat(PUB_HEARTBEAT_MS);
  net.begin(ssid, password, mqtt_server, mqtt_port,
            mqtt_user, mqtt_pass, mqtt_client_id, topicStatus, 0);
  net.setCallback(mqttCallback);
  net.setOnConnect(onMqttConnect);
  net.connectWifiBlocking();
  net.reconnect();

  if (ENABLE_SLIDESHOW && show.ready() && net.wifiOk()) {
    show.fetchList();
    lastListFetch = millis();
    if (show.count() > 0) { show.showNext(); lastImageChange = millis(); }
  }

  Serial.printf("[HEAP] μετά το setup: free=%u maxBlock=%u\n",
                ESP.getFreeHeap(), Kolona::largestFreeBlock());

  Kolona::safetyBegin();
}

void loop() {
  unsigned long now = millis();

  Kolona::safetyLoop();     // 1. δίχτυ ασφαλείας
  net.loop();               // 2. WiFi + MQTT χωρίς μπλοκάρισμα
  serviceBME();             // 3. BME688 χωρίς delay
  serviceDetection();       // 4. laser + ράμπα φωτός

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

  // 6. Ανάγνωση αισθητήρων + δημοσίευση
  if (now - previousMillis >= SENSOR_INTERVAL_MS) {
    previousMillis = now;

    if (bh1750Found && lightMeter.measurementReady()) {
      float lux = lightMeter.readLightLevel();
      if (lux >= 0) disp_lux = lux;          // κρατάμε την τελευταία έγκυρη
    }
    if (ltr390Found) disp_uv = ltr390.readOriginalData() / 2300.0f;

    disp_us    = readDistance();
    disp_pir   = digitalRead(PIR_PIN);
    rainVal    = digitalRead(RAIN_PIN);
    airQualVal = analogRead(AIR_QUAL_PIN);
    gasVal     = analogRead(GAS_PIN);
    ldrVal     = analogRead(LDR_PIN);

    sendLoRa();
    publishSensors();
  }

  // 7. Οθόνη
  if (now - lastBannerText > 500)                { lastBannerText = now; updateBannerText(); }
  if (now - lastBannerUpdate > BANNER_UPDATE_MS) { lastBannerUpdate = now; banner.tick(BANNER_STEP_PX); }
  if (now - lastTopBarUpdate > TOPBAR_UPDATE_MS) { lastTopBarUpdate = now; drawTopBar(); }

  // 8. Debug (1/δευτ.)
  static unsigned long lastDbg = 0;
  if (now - lastDbg > 1000) {
    lastDbg = now;
    Serial.printf("lux=%.1f DARK=%d MODE=%s tof=%d base=%d trig=%d pir=%d pwm=%d | free=%u maxBlock=%u\n",
                  disp_lux, night.value() ? 1 : 0, led.modeString(),
                  disp_tof, detect.lastValidMm(), disp_laserTrig ? 1 : 0,
                  digitalRead(PIR_PIN), led.pwm(),
                  ESP.getFreeHeap(), Kolona::largestFreeBlock());
  }
}
