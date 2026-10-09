/*
 * ESP-EYE MJPEG streaming server -> Home Assistant
 * Stream URL: http://<IP-του-board>:81/stream
 *
 * Arduino IDE setup:
 *   - Board:  "ESP32 Dev Module"  (ή "ESP-EYE" αν υπάρχει)
 *   - PSRAM:  Enabled            (ΥΠΟΧΡΕΩΤΙΚΟ)
 *   - Partition Scheme: Huge APP (3MB No OTA)
 *   - Library: "esp32" board package by Espressif
 */

#include "secrets.h"      // WiFi / MQTT credentials (not in git) - see secrets.example.h
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ---------- WiFi ----------
const char* ssid  = WIFI_SSID;
const char* password  = WIFI_PASSWORD;

// ---------- Pin map για ESP-EYE ----------
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   4
#define SIOD_GPIO_NUM  18
#define SIOC_GPIO_NUM  23
#define Y9_GPIO_NUM    36
#define Y8_GPIO_NUM    37
#define Y7_GPIO_NUM    38
#define Y6_GPIO_NUM    39
#define Y5_GPIO_NUM    35
#define Y4_GPIO_NUM    14
#define Y3_GPIO_NUM    13
#define Y2_GPIO_NUM    34
#define VSYNC_GPIO_NUM  5
#define HREF_GPIO_NUM  27
#define PCLK_GPIO_NUM  25

// ---------- MJPEG server ----------
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];

  res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) { res = ESP_FAIL; break; }

    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, 64, STREAM_PART, fb->len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    }
    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;   // ο client έκλεισε το stream
  }
  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;

  httpd_uri_t stream_uri = {
    .uri      = "/stream",
    .method   = HTTP_GET,
    .handler  = stream_handler,
    .user_ctx = NULL
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println("Stream server ready.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  delay(300);

  bool hasPsram = psramFound();
  Serial.printf("PSRAM found: %s\n", hasPsram ? "YES" : "NO");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (hasPsram) {
    // Με PSRAM: υψηλή ανάλυση, buffer στην PSRAM
    config.frame_size   = FRAMESIZE_UXGA;   // 1600x1200
    config.jpeg_quality = 10;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_LATEST;
  } else {
    // ΧΩΡΙΣ PSRAM: χαμηλή ανάλυση, buffer στην εσωτερική RAM -> ΔΕΝ σκάει
    config.frame_size   = FRAMESIZE_QVGA;   // 320x240
    config.jpeg_quality = 12;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }
  Serial.println("Camera OK.");
  // --- Αναστροφή εικόνας ---
  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);     // αναποδογύρισμα πάνω-κάτω
  s->set_hmirror(s, 1);   // καθρέφτισμα δεξιά-αριστερά

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Stream: http://");
  Serial.print(WiFi.localIP());
  Serial.println(":81/stream");

  startCameraServer();
}

void loop() {
  delay(10000);   // όλη η δουλειά γίνεται στον http server
}
