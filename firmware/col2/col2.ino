/* ============================================================================
   ΚΟΛΟΝΑ 2  (COL2)  -  ESP32-S3
   ----------------------------------------------------------------------------
   Ο κοινός κώδικας (ασφάλεια, φως/κύμα, ανίχνευση, δίκτυο, publish) είναι στη
   βιβλιοθήκη KolonaCore. Εδώ μένει ΜΟΝΟ ό,τι αφορά αυτή την κολόνα:
   ρυθμίσεις, pins, topics, οι δικοί της αισθητήρες.

   ΔΟΜΗ:  1. ΡΥΘΜΙΣΕΙΣ  2. PINS  3. ΔΙΚΤΥΟ/TOPICS  4. ΑΝΤΙΚΕΙΜΕΝΑ
          5. ΑΙΣΘΗΤΗΡΕΣ  6. MQTT  7. ΔΗΜΟΣΙΕΥΣΗ  8. setup/loop
   ============================================================================ */

#include "secrets.h"      // WiFi / MQTT credentials (not in git) - see secrets.example.h
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_BME280.h>
#include "Adafruit_SGP30.h"
#include <KolonaCore.h>

/* ===========================================================================
   1. ΡΥΘΜΙΣΕΙΣ   <-- ΜΟΝΟ ΕΔΩ ΑΛΛΑΖΕΙΣ
   =========================================================================== */
const int MY_INDEX = 2;                // θέση στον δρόμο (για το κύμα)

// ---- Ανίχνευση (laser TOF) ----
const int TRIGGER_DISTANCE_MM = 200;   // 200 = 20cm
const int NEAR_CONFIRM        = 2;     // συνεχόμενα κοντινά δείγματα

// ---- Μέρα / Νύχτα (LDR2). Χαμηλή τιμή = σκοτάδι ----
// ΒΑΘΜΟΝΟΜΗΜΕΝΟ ΜΕ ΠΡΑΓΜΑΤΙΚΕΣ ΜΕΤΡΗΣΕΙΣ (14/09/2026, ιστορικό Home Assistant):
//    φως αναμμένο -> ~3230
//    φως σβηστό   -> ~1715-2120
// Τα παλιά κατώφλια ήταν 500/800: ο αισθητήρας ΔΕΝ έφτανε ποτέ τόσο χαμηλά,
// οπότε η κολόνα νόμιζε ότι είναι πάντα μέρα και το LED δεν άναβε ΠΟΤΕ.
const int LDR2_DARK_ON  = 2500;        // < από αυτό -> ΝΥΧΤΑ (άναψε)
const int LDR2_DARK_OFF = 2900;        // > από αυτό -> ΜΕΡΑ (σβήσε). 2500..2900 = νεκρή ζώνη

// ---- LDR2 -> ποσοστό φωτεινότητας (ΜΟΝΟ για εμφάνιση) ----
const int LDR_RAW_DARK   = 300;
const int LDR_RAW_BRIGHT = 3500;

// ---- Χρόνοι φωτισμού ----
const unsigned long MOTION_HOLD_MS = 7000;   // 100% μετά από ανίχνευση
const unsigned long WAVE_HOLD_MS   = 2500;   // 78% από κύμα γείτονα

// ---- Ρυθμοί (ms) ----
const unsigned long PUBLISH_MS       = 500;
const unsigned long LED_UPDATE_MS    = 20;
const unsigned long TOF_READ_MS      = 80;
const unsigned long SGP_READ_MS      = 1000;
const unsigned long PUB_HEARTBEAT_MS = 10000;

#define LED_ACTIVE_LOW  false

/* ===========================================================================
   2. PINS
   =========================================================================== */
#define LORA_SCK   36
#define LORA_MOSI  35
#define LORA_MISO  37
#define LORA_CS    11
#define LORA_RST   38
#define LORA_DIO0  39
#define LORA_BAND  433E6

#define WATER_LEVEL_PIN  2      // όλα τα αναλογικά σε ADC1 -> δουλεύουν με WiFi ON
#define GAS_PIN          1
#define FLAME_PIN        7
#define AUDIO_PIN        10
#define LDR1_PIN         3
#define LDR2_PIN         4

#define HC_TRIG          5
#define HC_ECHO          6
#define LED_PIN          12

#define I2C_SDA          8
#define I2C_SCL          9
#define BME_ADDRESS      0x77
// Ο SGP30 έχει ΣΤΑΘΕΡΗ διεύθυνση I2C 0x58

/* ===========================================================================
   3. WiFi / MQTT / TOPICS   (τα στοιχεία σύνδεσης είναι στο secrets.h)
   =========================================================================== */
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

const char* mqtt_server    = MQTT_SERVER;
const int   mqtt_port      = 1883;
const char* mqtt_user      = MQTT_USER;
const char* mqtt_pass      = MQTT_PASSWORD;
const char* mqtt_client_id = "ESP32S3_col2_final";

const char* topicStatus      = "esp32s3/col2/status";
const char* topicWater       = "esp32s3/col2/sensors/water_level";
const char* topicGas         = "esp32s3/col2/sensors/gas";
const char* topicFlame       = "esp32s3/col2/sensors/flame";
const char* topicAudio       = "esp32s3/col2/sensors/audio";
const char* topicLDR1        = "esp32s3/col2/sensors/ldr";
const char* topicLDR2        = "esp32s3/col2/sensors/ldr2";
const char* topicUS          = "esp32s3/col2/sensors/distance_us_cm";
const char* topicTOF         = "esp32s3/col2/sensors/distance_tof_mm";
const char* topicLightLevel  = "esp32s3/col2/sensors/light_level";
const char* topicBrightness  = "esp32s3/col2/sensors/brightness_pct";
const char* topicBmeTemp     = "esp32s3/col2/sensors/bme_temperature";
const char* topicBmeHum      = "esp32s3/col2/sensors/bme_humidity";
const char* topicBmePress    = "esp32s3/col2/sensors/bme_pressure";
const char* topicEco2        = "esp32s3/col2/sensors/eco2";
const char* topicTvoc        = "esp32s3/col2/sensors/tvoc";
const char* topicLedCmd      = "esp32s3/col2/led/cmd";
const char* topicLedState    = "esp32s3/col2/led/state";
const char* topicLedMode     = "esp32s3/col2/led/mode";
const char* topicLedPwm      = "esp32s3/col2/led/pwm";
const char* topicLaserTrig   = "esp32s3/col2/sensors/laser_trigger";
const char* topicWaveTx      = "esp32s3/wave/event";
const char* topicLoRaRaw     = "esp32s3/col2/lora/payload";

/* ===========================================================================
   4. ΑΝΤΙΚΕΙΜΕΝΑ & ΚΑΤΑΣΤΑΣΗ
   =========================================================================== */
Adafruit_VL53L0X lox;
Adafruit_BME280  bme;
Adafruit_SGP30   sgp;

Kolona::Net        net;                 // ΠΡΙΝ τον Publisher (κρατάει reference)
Kolona::Led        led;
Kolona::Detect     detect;
Kolona::Hysteresis night;

enum PubId {
  P_WATER, P_GAS, P_FLAME, P_AUDIO, P_LDR1, P_LDR2, P_US, P_TOF,
  P_LIGHT, P_BRIGHT, P_BTEMP, P_BHUM, P_BPRES, P_ECO2, P_TVOC,
  P_LASER, P_LEDSTATE, P_LEDMODE, P_LEDPWM, P_COUNT
};
Kolona::Publisher<P_COUNT> pub(net.mqtt());

bool loxFound = false, bmeFound = false, sgpFound = false, loraOk = false;

int  lastWaterLevel = 0, lastGasValue = 0, lastFlameValue = 0, lastAudioValue = 0;
int  lastLdr1Value = 0,  lastLdr2Value = 0;
long lastDistCm = -1;
int  lastTofMm  = -1;
float lastBmeTemp = 0.0f, lastBmeHum = 0.0f, lastBmePress = 0.0f;
uint16_t lastEco2 = 400, lastTvoc = 0;
bool lastLaserTrig = false;

char g_payload[192];
char g_lastPayload[192] = "";
unsigned long g_lastPayloadMs = 0;

unsigned long lastPublish = 0, lastLedUpdate = 0, lastTofRead = 0, lastSgpRead = 0;
volatile bool g_ledStatusDirty = false;   // αίτημα από το mqttCallback

/* ===========================================================================
   5. ΑΙΣΘΗΤΗΡΕΣ
   =========================================================================== */
long readHCSR04() {
  digitalWrite(HC_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(HC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(HC_TRIG, LOW);
  long duration = pulseIn(HC_ECHO, HIGH, 25000);
  if (duration == 0) return -1;
  return (long)((duration * 0.0343f) / 2.0f);
}

// KY-038: μία analogRead πέφτει σε τυχαίο σημείο του κύματος και δείχνει μόνο
// το DC επίπεδο (~1620). Δειγματοληπτούμε σε ΚΑΘΕ loop και κρατάμε min/max·
// η ένταση = max - min στο διάστημα δημοσίευσης (0 = ησυχία, μεγάλο = θόρυβος).
int audioMin = 4095, audioMax = 0;
void sampleAudio() {
  int v = analogRead(AUDIO_PIN);
  if (v < audioMin) audioMin = v;
  if (v > audioMax) audioMax = v;
}
int takeAudioLevel() {
  int level = (audioMax >= audioMin) ? audioMax - audioMin : 0;
  audioMin = 4095; audioMax = 0;
  return level;
}

// Ο SGP30 δίνει ακριβέστερο eCO2/TVOC με την "απόλυτη υγρασία" από το BME280.
uint32_t absoluteHumidity(float t, float rh) {
  const float a = 216.7f * ((rh / 100.0f) * 6.112f *
                   exp((17.62f * t) / (243.12f + t)) / (273.15f + t));
  return (uint32_t)(1000.0f * a);   // [mg/m^3]
}

int ldrToPct(int raw) {
  return map(constrain(raw, LDR_RAW_DARK, LDR_RAW_BRIGHT), LDR_RAW_DARK, LDR_RAW_BRIGHT, 0, 100);
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
  if (net.mqttOk()) net.mqtt().publish(topicWaveTx, "col2", false);
}

void onMqttConnect() {
  net.mqtt().subscribe(topicLedCmd);
  net.mqtt().subscribe(topicWaveTx);
  pub.forceAll();                 // ξαναστείλε τα πάντα μετά την επανασύνδεση
  g_lastPayload[0] = '\0';
  publishLedStatus();
}

// ΠΡΟΣΟΧΗ: εδώ ΔΕΝ κάνουμε publish (φωλιασμένο publish μέσα σε mqtt.loop()
// είναι γνωστή πηγή αστάθειας). Σηκώνουμε σημαία και το loop το στέλνει.
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

/* ===========================================================================
   7. ΔΗΜΟΣΙΕΥΣΗ
   =========================================================================== */
// CSV payload (ίδια σειρά πεδίων με πριν, eco2/tvoc στο τέλος)
void buildPayload(bool laserTrig) {
  snprintf(g_payload, sizeof(g_payload),
           "%d,%d,%d,%d,%d,%d,%ld,%d,%.1f,%.1f,%.1f,%d,%d,%u,%u",
           lastWaterLevel, lastGasValue, lastFlameValue, lastAudioValue,
           lastLdr1Value, lastLdr2Value, lastDistCm, lastTofMm,
           lastBmeTemp, lastBmeHum, lastBmePress, led.pwm(), laserTrig ? 1 : 0,
           lastEco2, lastTvoc);
}

void publishAll(bool laserTrig) {
  if (!net.mqttOk()) return;

  pub.i(P_WATER,  topicWater,      lastWaterLevel, 20);   // ADC 12bit -> 20 counts
  pub.i(P_GAS,    topicGas,        lastGasValue,   20);
  pub.i(P_FLAME,  topicFlame,      lastFlameValue, 20);
  pub.i(P_AUDIO,  topicAudio,      lastAudioValue, 20);
  pub.i(P_LDR1,   topicLDR1,       lastLdr1Value,  20);
  pub.i(P_LDR2,   topicLDR2,       lastLdr2Value,  20);
  pub.l(P_US,     topicUS,         lastDistCm,      1);
  pub.i(P_TOF,    topicTOF,        lastTofMm,       5);
  pub.i(P_LIGHT,  topicLightLevel, lastLdr2Value,  20);
  pub.i(P_BRIGHT, topicBrightness, ldrToPct(lastLdr2Value), 1);

  if (bmeFound) {
    pub.f(P_BTEMP, topicBmeTemp,  lastBmeTemp,  1, 0.1f);
    pub.f(P_BHUM,  topicBmeHum,   lastBmeHum,   1, 0.3f);
    pub.f(P_BPRES, topicBmePress, lastBmePress, 1, 0.2f);
  }
  if (sgpFound) {
    pub.i(P_ECO2, topicEco2, lastEco2, 5);
    pub.i(P_TVOC, topicTvoc, lastTvoc, 2);
  }

  pub.t(P_LASER, topicLaserTrig, laserTrig, "ON", "OFF");
  publishLedStatus();

  // Το CSV payload στέλνεται μόνο όταν αλλάζει (ή κάθε PUB_HEARTBEAT_MS)
  unsigned long now = millis();
  if (strcmp(g_lastPayload, g_payload) != 0 || now - g_lastPayloadMs > PUB_HEARTBEAT_MS) {
    strncpy(g_lastPayload, g_payload, sizeof(g_lastPayload) - 1);
    g_lastPayload[sizeof(g_lastPayload) - 1] = '\0';
    g_lastPayloadMs = now;
    net.mqtt().publish(topicLoRaRaw, g_payload, true);
  }
}

/* ===========================================================================
   8. setup() / loop()
   =========================================================================== */
void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n===== COL2 START =====");
  Kolona::printResetReason();

  pinMode(HC_TRIG, OUTPUT);
  pinMode(HC_ECHO, INPUT);
  digitalWrite(HC_TRIG, LOW);
  analogReadResolution(12);

  // --- Φως / ανίχνευση ---
  led.begin(LED_PIN, 5000, 8, LED_ACTIVE_LOW);
  led.setHold(MOTION_HOLD_MS, WAVE_HOLD_MS);
  led.setWaveSender(sendWave);
  detect.configure(TRIGGER_DISTANCE_MM, NEAR_CONFIRM);
  night.configure(LDR2_DARK_ON, LDR2_DARK_OFF);

  // --- I2C + αισθητήρες ---
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setTimeOut(50);              // δεν κολλάει για πάντα αν γλιστρήσει ο bus

  loxFound = lox.begin();
  Serial.println(loxFound ? "VL53L0X OK" : "VL53L0X FAIL");

  bmeFound = bme.begin(BME_ADDRESS);
  Serial.println(bmeFound ? "BME280 OK" : "BME280 FAIL");

  sgpFound = sgp.begin();
  if (sgpFound) Serial.printf("SGP30 OK serial #%X%X%X\n",
                              sgp.serialnumber[0], sgp.serialnumber[1], sgp.serialnumber[2]);
  else          Serial.println("SGP30 FAIL");

  // --- LoRa ---
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  loraOk = LoRa.begin(LORA_BAND);
  Serial.println(loraOk ? "LoRa OK" : "LoRa FAIL");

  // --- Δίκτυο ---
  pub.setHeartbeat(PUB_HEARTBEAT_MS);
  net.begin(ssid, password, mqtt_server, mqtt_port,
            mqtt_user, mqtt_pass, mqtt_client_id, topicStatus, 1);
  net.setCallback(mqttCallback);
  net.setOnConnect(onMqttConnect);
  net.connectWifiBlocking();
  net.reconnect();

  lastLdr2Value = analogRead(LDR2_PIN);

  Serial.printf("[HEAP] μετά το setup: free=%u maxBlock=%u\n",
                ESP.getFreeHeap(), Kolona::largestFreeBlock());

  Kolona::safetyBegin();
}

void loop() {
  unsigned long now = millis();

  Kolona::safetyLoop();     // 1. δίχτυ ασφαλείας
  net.loop();               // 2. WiFi + MQTT χωρίς μπλοκάρισμα

  if (g_ledStatusDirty && net.mqttOk()) { g_ledStatusDirty = false; publishLedStatus(); }

  sampleAudio();            // ήχος: συνεχής δειγματοληψία για peak-to-peak

  // 3. Laser (συχνά)
  if (loxFound && now - lastTofRead >= TOF_READ_MS) {
    lastTofRead = now;
    VL53L0X_RangingMeasurementData_t m;
    lox.rangingTest(&m, false);
    lastTofMm = (m.RangeStatus == 0) ? m.RangeMilliMeter : -1;
  }

  // 4. SGP30 κάθε 1s (με αντιστάθμιση υγρασίας από BME280)
  if (sgpFound && now - lastSgpRead >= SGP_READ_MS) {
    lastSgpRead = now;
    if (bmeFound && lastBmeTemp > -40 && lastBmeHum > 0)
      sgp.setHumidity(absoluteHumidity(lastBmeTemp, lastBmeHum));
    if (sgp.IAQmeasure()) { lastEco2 = sgp.eCO2; lastTvoc = sgp.TVOC; }
  }

  // 5. Έλεγχος φωτός σε ΣΤΑΘΕΡΟ ρυθμό 20ms -> ομαλή ράμπα
  if (now - lastLedUpdate >= LED_UPDATE_MS) {
    lastLedUpdate = now;
    lastLdr2Value = analogRead(LDR2_PIN);
    lastLaserTrig = detect.update(lastTofMm);
    led.update(night.update(lastLdr2Value), lastLaserTrig);
  }

  // 6. Ανάγνωση υπόλοιπων αισθητήρων + δημοσίευση
  if (now - lastPublish >= PUBLISH_MS) {
    lastPublish = now;

    lastWaterLevel = analogRead(WATER_LEVEL_PIN);
    lastGasValue   = analogRead(GAS_PIN);
    lastFlameValue = analogRead(FLAME_PIN);
    lastAudioValue = takeAudioLevel();
    lastLdr1Value  = analogRead(LDR1_PIN);
    lastDistCm     = readHCSR04();

    if (bmeFound) {
      lastBmeTemp  = bme.readTemperature();
      lastBmeHum   = bme.readHumidity();
      lastBmePress = bme.readPressure() / 100.0F;
    }

    buildPayload(lastLaserTrig);
    publishAll(lastLaserTrig);

    if (loraOk) {
      LoRa.beginPacket();
      LoRa.print(g_payload);
      LoRa.endPacket();
    }
  }

  // 7. Debug (1/δευτ.)
  static unsigned long lastDbg = 0;
  if (now - lastDbg > 1000) {
    lastDbg = now;
    Serial.printf("LDR2=%d DARK=%d MODE=%s TOF=%d TRIG=%d PWM=%d eCO2=%u TVOC=%u "
                  "Gas=%d Flame=%d AUDIO=%d | free=%u maxBlock=%u\n",
                  lastLdr2Value, night.value() ? 1 : 0, led.modeString(), lastTofMm,
                  lastLaserTrig ? 1 : 0, led.pwm(), lastEco2, lastTvoc,
                  lastGasValue, lastFlameValue, lastAudioValue,
                  ESP.getFreeHeap(), Kolona::largestFreeBlock());
  }
}
