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
#include "secrets.h"    // secrets_example.h for reference
#include "settings.h"


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


// === sync with current time ===
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


void sendImageToTelegram() {
    // --- skip certificate validation ---
    telegramClient.setInsecure();

    // --- open latest ring capture JPEG ---
    File file = SD_MMC.open("/IMG_" + latestRingCapture_filename + ".jpg");
    if (!file) {
        DBG_PRINTLN("ERROR: Failed to open JPEG");
        while(1);
    }
    else {
        DBG_PRINTLN("Opened JPEG: " + String(file.name()));
    }

    // --- URL endpoint ---
    String url = "/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendPhoto";

    // --- multipart boundary ---
    String boundary = "----ESP32CAMBoundary";

    // --- build multipart body ---
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

    // --- build multipart tail ---
    String tail = "\r\n--" + boundary + "--\r\n";

    // --- determine total size of content ---
    uint32_t totalLength = head.length() + file.size() + tail.length();

    // --- connect to telegram ---
    const char* telegramHost = "api.telegram.org";
    DBG_PRINTLN("Connecting to " + String(telegramHost));
    if (!telegramClient.connect(telegramHost, 443)) {
        DBG_PRINTLN("ERROR: Telegram connection failed");
        file.close();
        return;
    }

    // --- send HTTP POST headers ---
    telegramClient.print(
        "POST " + url + " HTTP/1.1\r\n"
        "Host: " + String(telegramHost) + "\r\n"
        "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
        "Content-Length: " + String(totalLength) + "\r\n"
        "Connection: close\r\n\r\n"
    );

    // --- send multipart head ---
    telegramClient.print(head);

    // --- send file binary ---
    uint8_t buf[1024];
    DBG_PRINTLN("Sending JPEG to telegram...");
    while (file.available()) {
        int n = file.read(buf, sizeof(buf));
        telegramClient.write(buf, n);
    }

    // --- send multipart tail ---
    telegramClient.print(tail);

    // -- close file ---
    file.close();

    // --- read telegram response ---
    DBG_PRINTLN("Telegram response:");
    while (telegramClient.connected()) {
        String line = telegramClient.readStringUntil('\n');
        if (line == "\r") break;
    }
    String body = telegramClient.readString();
    DBG_PRINTLN(body);

    DBG_PRINTLN("JPEG sent");
}



void ringIfRung(unsigned long checkDuration = 100) {
    unsigned long start = millis();

    while (millis() - start < checkDuration) {
        if (mcp.digitalRead(BTN_PIN) == LOW && millis() - lastRing_endTime > gapSince_lastRing) {
            DBG_PRINTLN("Bell rung!");
            
            ensureMqtt();
            mqtt.publish("doorbell/ring", "pressed");

            captureAndSaveImage(latestRingCapture_filename);

            sendImageToTelegram();

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


bool checkIfUploadTIme() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        DBG_PRINTLN("ERROR: Failed to obtain time");
        return false;
    }

    // Extract the hour from the current time
    int currentHour = timeinfo.tm_hour;

    // Check if the current time is between 1 AM and 4 AM
    if (currentHour >= 16 || currentHour < 4) {
        return true;
    } else {
        return false;
    }
}


bool uploadImageToCloudinary(File &file, String filename) {
    // --- skip certificate validation ---
    cloudinaryClient.setInsecure();

    // --- URL endpoint ---
    String url = "/v1_1/" + String(CLOUDINARY_CLOUD_NAME) + "/image/upload";

    // --- multipart boundary ---
    String boundary = "----ESP32CloudinaryBoundary";

    // --- build multipart body ---
    String head =
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"upload_preset\"\r\n\r\n" +
        String(CLOUDINARY_UPLOAD_PRESET) + "\r\n" +

        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"public_id\"\r\n\r\n" +
        filename + "\r\n" +

        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n";

    // --- build multipart tail ---
    String tail = "\r\n--" + boundary + "--\r\n";

    // --- determine total size of content ---
    uint32_t totalLength = head.length() + file.size() + tail.length();

    // --- connect to telegram ---
    DBG_PRINTLN("Connecting to " + String(cloudinaryHost));
    if (!cloudinaryClient.connect(cloudinaryHost, 443)) {
        DBG_PRINTLN("ERROR: Cloudinary connection failed");
        file.close();
        return false;
    }

    // --- send HTTP POST headers ---
    cloudinaryClient.print(
        "POST " + url + " HTTP/1.1\r\n"
        "Host: " + String(cloudinaryHost) + "\r\n"
        "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
        "Content-Length: " + String(totalLength) + "\r\n"
        "Connection: close\r\n\r\n"
    );

    // --- send multipart head ---
    cloudinaryClient.print(head);

    // --- send file binary ---
    uint8_t buf[1024];
    DBG_PRINTLN("Uploading JPEG to cloudinary...");
    while (file.available()) {
        int n = file.read(buf, sizeof(buf));
        cloudinaryClient.write(buf, n);
    }

    // --- send multipart tail ---
    cloudinaryClient.print(tail);

    // -- close file ---
    file.close();

    // --- read cloudinary response ---
    DBG_PRINTLN("Cloudinary response:");
    while (cloudinaryClient.connected()) {
        String line = cloudinaryClient.readStringUntil('\n');
        if (line == "\r") break;
    }
    String body = cloudinaryClient.readString();
    DBG_PRINTLN(body);

    DBG_PRINTLN("JPEG uploaded");
    return true;
}


bool uploadAndDeleteAll() {
    // --- open SD root directory ---
    File root = SD_MMC.open("/");
    if (!root || !root.isDirectory()) {
        DBG_PRINTLN("ERROR: SD root open failed");
        lastAction_endTime = millis();
        return false;
    }

    // --- open next available file in root ---
    File file = root.openNextFile();

    // --- loop through files ---
    while (file) {
        delay(100);

        // --- skip folders, non-JPEGs and the latest ring capture ---
        String filename = file.name();
        if (file.isDirectory() || !filename.endsWith(".jpg") || filename == "IMG_" + latestRingCapture_filename + ".jpg") {
            file = root.openNextFile();
            continue;
        }

        // --- upload JPEG to cloudinary ---
        bool ok = uploadImageToCloudinary(file, filename);

        if (ok) {
            DBG_PRINTLN("Upload OK deleting " + filename + " from SD card");
            SD_MMC.remove("/" + filename);
        }
        else {
            DBG_PRINTLN("Upload failed, stopping uploads");
            lastAction_endTime = millis();
            return false;
        }

        // --- stop if motion detected ---
        if (mcp.digitalRead(PIR_PIN) == HIGH) {
            DBG_PRINTLN("Motion detected, stopping uploads");
            lastAction_endTime = millis();
            return false;
        }

        file = root.openNextFile();
        delay(100);
    }

    DBG_PRINTLN("All images uploaded successfully!");
    lastAction_endTime = millis();
    return true;
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
    else if (uploadCheck == false && checkIfUploadTIme() == true) {
        uploadCheck = uploadAndDeleteAll();
    }
    else if (millis() - lastAction_endTime >= standbyPeriod) {
        DBG_PRINTLN("ESP32-CAM entering deep sleep");

        DBG_DELAY(1000);

        gpio_hold_en((gpio_num_t)BLUE_LED_PIN);

        esp_deep_sleep_start();
    }
}
