#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include <UniversalTelegramBot.h>

// ========= CONFIG =========
const char* ssid = "";
const char* password = "";

String BOTtoken = "";
String CHAT_ID  = "";

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

/*
// ========= CAMERA PIN CONFIG (ESP32-CAM AI-Thinker) =========
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
*/

WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

// ========= FUNCTIONS =========
void startCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_SVGA; // 800x600
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
}

void sendPhoto() {
  Serial.println("Taking picture...");

  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  Serial.println("Connecting to Telegram...");
  if (clientTCP.connect("api.telegram.org", 443)) {
    String head = "--boundary\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + CHAT_ID + "\r\n--boundary\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--boundary--\r\n";

    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;

    clientTCP.printf("POST /bot%s/sendPhoto HTTP/1.1\r\n", BOTtoken.c_str());
    clientTCP.println("Host: api.telegram.org");
    clientTCP.println("Content-Type: multipart/form-data; boundary=boundary");
    clientTCP.print("Content-Length: ");
    clientTCP.println(totalLen);
    clientTCP.println();
    clientTCP.print(head);

    clientTCP.write(fb->buf, fb->len);
    clientTCP.print(tail);

    String response;
    while (clientTCP.connected() || clientTCP.available()) {
      if (clientTCP.available()) {
        response = clientTCP.readString();
      }
    }
    Serial.println(response);
  }
  else {
    Serial.println("Connection to Telegram failed");
  }

  esp_camera_fb_return(fb);
}

// ========= SETUP =========
void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);

  WiFi.begin(ssid, password);
  clientTCP.setInsecure(); // For HTTPS
  Serial.print("Connecting WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  startCamera();
  bot.sendMessage(CHAT_ID, "ESP32-CAM Ready", "");
}

// ========= LOOP =========
void loop() {
  if (digitalRead(PIR_PIN) == HIGH) {
    Serial.println("Motion detected!");
    bot.sendMessage(CHAT_ID, "Motion detected! Sending picture...", "");
    sendPhoto();
    delay(10000);
  }
}

