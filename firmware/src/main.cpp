#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "esp_camera.h"
#include "SD_MMC.h"
#include "WiFi.h"
#include "esp_sleep.h"
#include "PubSubClient.h"
#include <time.h>
#include <HTTPClient.h>
#include "secrets.h"    // check secrets_example.h for reference


// ===== debug configuration =====
#define SERIAL_DEBUG 1   // set to 1 or 0 to turn serial debugging ON or OFF respectively

#if SERIAL_DEBUG
  #define DBG_SERIAL_BEGIN(x)   Serial.begin(x)
  #define DBG_PRINT(x)          Serial.print(x)
  #define DBG_PRINTLN(x)        Serial.println(x)
  #define DBG_DELAY(X)          delay(X)
#else
  #define DBG_BEGIN_SERIAL(x)
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
  #define DBG_DELAY(X)
#endif


// === ESP32-CAM pin configuration ===
// source pin to wake ESP32-CAM from deep-sleep
const int WAKE_PIN = 4;

// I2C pins
const int SDA_PIN = 13;          // I2C SDA to MCP23X17
const int SCL_PIN = 12;          // I2C SCL to MCP23X17

// camera pins
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22


// === MCP23017 configuration ===
Adafruit_MCP23X17 mcp;

// wake & input sources
const int BTN_PIN = 0;          // push button input
const int PIR_PIN = 1;          // PIR input

// peripherals
const int RED_LED_PIN   = 3;    // external red LED
const int BLUE_LED_PIN  = 4;    // external blue LED
const int BUZZER_PIN  = 5;      // piezo buzzer


// === time configuration ===
const char* ntpServer = "uk.pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;


// === filename of image captured at latest doorbell ring ===
const String latestRingCapture_filename = "latest_ring_capture";


// === time variables ===
unsigned long boot_startTime            = 0;  // time at start of boot
unsigned long initPIR_startTime         = 0;  // time at start of PIR initialsation
unsigned long lastAction_endTime        = 0;  // time when last action ended
unsigned long lastRing_endTime          = 0;
unsigned long surveillance_startTime    = 0; // time at start of surveillance

const unsigned long surveillancePeriod  = 10000; // allowed suveillance duration
const unsigned long initPIR_period      = 30000; // duration of initialising PIR sensor
const unsigned long standbyPeriod       = 200000; // allowed standby duration
const unsigned long gapSince_lastRing   = 2000;


// === MQTT Setup ===
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);


// === blink infinitely ===
void error(int LED_PIN) {
    while(true){
        digitalWrite(LED_PIN, LOW);
        delay(200);
        digitalWrite(LED_PIN, HIGH);
        delay(200);
    }
}


// === initialiase MCP23017 GPIO expander ===
void initMCP(){
    Wire.begin(SDA_PIN, SCL_PIN);

    if (!mcp.begin_I2C(0x20, &Wire)) {
        Serial.println("MCP23X17 init failed");
        while (1);
    }
}


// === initialise camera ===
void initCamera() {
    DBG_PRINTLN("Initialising camera");

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
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;

    if (esp_camera_init(&config) != ESP_OK) {
        DBG_PRINTLN("ERROR: Camera init failed");
        while(1);
    }
    else {
        DBG_PRINTLN("Camera initialised!");
    }
}


// === initialise micro SD card ===
void initMicroSD() {
    DBG_PRINTLN("Initialising SD card");

    if (!SD_MMC.begin("/sdcard", true)) {
        DBG_PRINTLN("ERROR: Failed to initialise SD card");
        while(1);
    }
    else {
        DBG_PRINTLN("SD card initialised!");
    }

    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        DBG_PRINTLN("ERROR: No SD Card detected");
        while(1);
    }
}


// === initialise Wi-Fi ===
void initWifi() {
    if (WiFi.status() != WL_CONNECTED) {
        DBG_PRINT("Connecting to WiFi");

        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASS);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
            delay(10);
            DBG_PRINT(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            DBG_PRINTLN("");
            DBG_PRINT("WiFi connected: ");
            DBG_PRINTLN(WiFi.localIP());
        } else {
            DBG_PRINTLN("");
            DBG_PRINTLN("ERROR: WiFi connection failed");
            while(1);
        }
    }
}



void initTime() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    delay(5000);

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        DBG_PRINTLN("ERROR: Failed to obtain time");
        while(1);
    }

    char timeNow[40];
    strftime(timeNow, sizeof(timeNow), "%Y-%m-%d_%H-%M-%S", &timeinfo);

    DBG_PRINTLN("Time synchronized via NTP @ " + String(timeNow));
}


// === blocking delay to give time for PIR sensor to initialise ===
void initPIR() {
    initPIR_startTime = millis();
    while (millis() - initPIR_startTime < initPIR_period) {
        mcp.digitalWrite(BLUE_LED_PIN, HIGH);
        delay(500);
        mcp.digitalWrite(BLUE_LED_PIN, LOW);
        delay(500);
    }
    lastAction_endTime = millis();
}



void ensureMqtt() {
    while (!mqtt.connected()) {
        DBG_PRINTLN("Not connected to MQTT");
        delay(50);
        mqtt.connect(
            "smart-doorbell",
            MQTT_USER,
            MQTT_PASS
        );
    }
    DBG_PRINTLN("Connected to MQTT!");
}


String getCurrentTimeString() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "UNKNOWN TIME";
    }

    char buffer[40];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &timeinfo);

    return String(buffer);
}


// === capture and save image ===
void captureAndSaveImage(String filename = getCurrentTimeString()) {
    
    // Discard first frame (stale)
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);

    delay(50);
    
    // Real capture
    fb = esp_camera_fb_get();
    if (!fb) {
        DBG_PRINTLN("capture failed");
        while(1);
    }

    String path = "/IMG_" + filename + ".jpg";
    Serial.printf("Picture file name: %s\n", path.c_str());

    fs::FS &fs = SD_MMC;
    File file = fs.open(path.c_str(), FILE_WRITE);

    if (!file) {
        Serial.printf("Failed to open file in writing mode");
        while(1);
    } else {
        file.write(fb->buf, fb->len);
        Serial.printf("Saved: %s\n", path.c_str());
    }

    file.close();
    esp_camera_fb_return(fb);

}



void sendPhotoToTelegram() {
    File file = SD_MMC.open("/IMG_" + latestRingCapture_filename + ".jpg");

    if (!file) {
        DBG_PRINTLN("Failed to open image file for Telegram");
        return;
    }

    String captionText = "🔔 Someone's at the door!";
    DBG_PRINTLN("Sending photo to Telegram...");

    WiFiClientSecure client;
    client.setInsecure(); // skip certificate validation

    String url = "/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendPhoto";
    String boundary = "----ESP32CAMBoundary";

    // Build multipart body (start)
    String head =
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
        String(TELEGRAM_CHAT_ID) + "\r\n" +
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" +
        captionText + "\r\n" +
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"photo\"; filename=\"doorbell.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n";

    String tail = "\r\n--" + boundary + "--\r\n";

    uint32_t totalLength = head.length() + file.size() + tail.length();

    // Connect to Telegram
    if (!client.connect("api.telegram.org", 443)) {
        DBG_PRINTLN("Telegram connection failed");
        file.close();
        return;
    }

    // Send HTTP POST headers
    client.print(
        "POST " + url + " HTTP/1.1\r\n"
        "Host: api.telegram.org\r\n"
        "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
        "Content-Length: " + String(totalLength) + "\r\n"
        "Connection: close\r\n\r\n"
    );

    // Send multipart head
    client.print(head);

    // Send file binary
    uint8_t buf[1024];
    while (file.available()) {
        int n = file.read(buf, sizeof(buf));
        client.write(buf, n);
    }

    // Send multipart tail
    client.print(tail);

    file.close();

    // Read Telegram response
    DBG_PRINTLN("Telegram response:");
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") break;
    }

    String body = client.readString();
    DBG_PRINTLN(body);
    DBG_PRINTLN("Done sending photo.");
}



void ringIfRung(unsigned long checkDuration = 100) {
    unsigned long start = millis();

    while (millis() - start < checkDuration) {
        if (mcp.digitalRead(BTN_PIN) == LOW && millis() - lastRing_endTime > gapSince_lastRing) {
            DBG_PRINTLN("Bell rung!");
            
            ensureMqtt();
            mqtt.publish("doorbell/ring", "pressed");

            captureAndSaveImage(latestRingCapture_filename);

            sendPhotoToTelegram();

            lastRing_endTime = millis();
            lastAction_endTime = millis();
        }
    }
}


// === activate surveillance routine ===
void activateSurveillance() {
    mcp.digitalWrite(BUZZER_PIN, HIGH);
    surveillance_startTime = millis();
    while (millis() - surveillance_startTime <= surveillancePeriod) {
        mcp.digitalWrite(RED_LED_PIN, HIGH);
        captureAndSaveImage();
        mcp.digitalWrite(RED_LED_PIN, LOW);
        ringIfRung();
        mcp.digitalWrite(BUZZER_PIN, LOW);
    }
    lastAction_endTime = millis();
}


void uploadToGoogleDrive() {
// TODO
}


// === initialize system and perform startup sequence ===
void setup() {
    DBG_DELAY(50);

    unsigned long boot_startTime = millis(); // record time at start of boot

    DBG_SERIAL_BEGIN(115200);

    initMCP();
    initCamera();
    initMicroSD();
    initWifi();

    mqtt.setServer(MQTT_HOST, MQTT_PORT);

    // set pinmodes
    mcp.pinMode(BLUE_LED_PIN, OUTPUT);
    mcp.pinMode(RED_LED_PIN, OUTPUT);
    mcp.pinMode(BUZZER_PIN, OUTPUT);
    mcp.pinMode(BTN_PIN, INPUT_PULLUP);
    mcp.pinMode(PIR_PIN, INPUT);
    pinMode(WAKE_PIN, INPUT_PULLDOWN);

    // set initial states of peripherals
    mcp.digitalWrite(BLUE_LED_PIN, HIGH);
    mcp.digitalWrite(RED_LED_PIN, LOW);
    mcp.digitalWrite(BUZZER_PIN, LOW);

    esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_PIN, 1);

    esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();

    DBG_PRINT("Boot duration: ");
    DBG_PRINTLN(millis() - boot_startTime);

    switch (wakeupReason) {
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            DBG_PRINTLN("Cold boot");
            initTime();
            initPIR();
            break;

        case ESP_SLEEP_WAKEUP_EXT0:
            DBG_PRINTLN("Wakeup by PIR");
            activateSurveillance();
            break;
        
        default:
            DBG_PRINTLN("Unknown wake reason");
            while(1);
            break;
    }

    DBG_PRINTLN("Runtime begin");
}


// === main runtime loop ===
void loop() {
    mqtt.loop();

    mcp.digitalWrite(BLUE_LED_PIN, HIGH);
    mcp.digitalWrite(RED_LED_PIN, LOW);
    mcp.digitalWrite(BUZZER_PIN, LOW);

    ringIfRung();

    if (mcp.digitalRead(PIR_PIN) == HIGH) {
        DBG_PRINTLN("Motion detected");
        activateSurveillance();
    }
    else if (millis() - lastAction_endTime >= standbyPeriod) {
        DBG_PRINTLN("ESP32-CAM entering deep sleep");

        DBG_DELAY(1000);

        gpio_hold_en((gpio_num_t)BLUE_LED_PIN);

        esp_deep_sleep_start();
    }
}
