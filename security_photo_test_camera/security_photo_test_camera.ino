#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_system.h"
#include <PubSubClient.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#define LED_FLASH 4

#define mqtt_server "192.168.1.64"
#define mqtt_user "mqtt_user"
#define mqtt_password ""

//Wifi
const char* ssid = "";
const char* password = "";

//Telegram credentials
String BOTtoken = "";
String chat_id  = "";

const char* telegram_host = "api.telegram.org";
WiFiClientSecure espclient;
PubSubClient client(espclient);

void setup() {
  Serial.begin(115200);
 
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, 1883);

  // Camera config
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

  pinMode(LED_FLASH, OUTPUT);

  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA; //Options: QQVGA/QVGA/VGA/SVGA/XGA/SXGA/UXGA
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("PsRam On");
  } else {
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("PsRam Off");
  }

  // Init camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_brightness(s, 2);              // -2 to +2
  s->set_contrast(s, 1);                // -2 to +2
  s->set_saturation(s, 1);              // -2 to +2
  s->set_gain_ctrl(s, 1);               // auto gain on
  s->set_exposure_ctrl(s, 1);           // auto exposure on
  s->set_whitebal(s, 1);                // auto white balance

  espclient.setInsecure(); // allow HTTPS without cert
  Serial.println("Setup done. Taking photo...");
  
  //delay(3000);
  //sendPhotoTelegram();  // Take and send one photo at startup
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect("ESP332CAM_lient", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  //delay(10000);
  //sendPhotoTelegram();
  //delay(10000);
}

void sendPhotoTelegram() {
  camera_fb_t * fb;
  for (int i = 0; i < 3; i++) {
      fb = esp_camera_fb_get();
      if (fb) {
          esp_camera_fb_return(fb); // discard
          fb = NULL;
      }
      delay(200); // let sensor adjust
  }

  digitalWrite(LED_FLASH, HIGH);  // LED ON
  delay(1000);

  fb = esp_camera_fb_get();
  
  digitalWrite(LED_FLASH, LOW);  // LED OFF

  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  
  if (!espclient.connect(telegram_host, 443)) {
    Serial.println("Connection to Telegram failed");
    esp_camera_fb_return(fb);
    fb = NULL;
    return;
  }

  String head = "--randomboundary\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + chat_id + "\r\n--randomboundary\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--randomboundary--\r\n";

  uint32_t imageLen = fb->len;
  uint32_t extraLen = head.length() + tail.length();
  uint32_t totalLen = imageLen + extraLen;

  espclient.println("POST /bot" + BOTtoken + "/sendPhoto HTTP/1.1");
  espclient.println("Host: " + String(telegram_host));
  espclient.println("Content-Type: multipart/form-data; boundary=randomboundary");
  espclient.println("Content-Length: " + String(totalLen));
  espclient.println();
  espclient.print(head);

  espclient.write(fb->buf, fb->len);
  espclient.print(tail);

  esp_camera_fb_return(fb);
  fb = NULL;

  String response;
  while (espclient.connected()) {
    String line = espclient.readStringUntil('\n');
    if (line == "\r") break;
  }
  while (espclient.available()) {
    char c = espclient.read();
    response += c;
  }
  Serial.println("Telegram response:");
  Serial.println(response);

  espclient.stop();
}