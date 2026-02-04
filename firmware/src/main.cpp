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


// === blink infinitely ===
void error(int LED_PIN) {
    while(true){
        digitalWrite(LED_PIN, LOW);
        delay(200);
        digitalWrite(LED_PIN, HIGH);
        delay(200);
    }
}


// === initialiase MCP23017 as a GPIO expander ===
void initMCP(){
    DBG_PRINTLN("Initialising MCP23017...");
    Wire.begin(SDA_PIN, SCL_PIN);

    if (!mcp.begin_I2C(0x20, &Wire)) {
        DBG_PRINTLN("ERROR: MCP23017 init failed");
        while (1);
    }
    else {
        DBG_PRINTLN("Initialised MCP23017");
    }
}


// === initialise camera ===
void initCamera() {
    DBG_PRINTLN("Initialising camera...");

    // --- configure camera ---
    camera_config_t config;
    config.ledc_channel     = LEDC_CHANNEL_0;
    config.ledc_timer       = LEDC_TIMER_0;
    config.pin_d0           = Y2_GPIO_NUM;
    config.pin_d1           = Y3_GPIO_NUM;
    config.pin_d2           = Y4_GPIO_NUM;
    config.pin_d3           = Y5_GPIO_NUM;
    config.pin_d4           = Y6_GPIO_NUM;
    config.pin_d5           = Y7_GPIO_NUM;
    config.pin_d6           = Y8_GPIO_NUM;
    config.pin_d7           = Y9_GPIO_NUM;
    config.pin_xclk         = XCLK_GPIO_NUM;
    config.pin_pclk         = PCLK_GPIO_NUM;
    config.pin_vsync        = VSYNC_GPIO_NUM;
    config.pin_href         = HREF_GPIO_NUM;
    config.pin_sccb_sda     = SIOD_GPIO_NUM;
    config.pin_sccb_scl     = SIOC_GPIO_NUM;
    config.pin_pwdn         = PWDN_GPIO_NUM;
    config.pin_reset        = RESET_GPIO_NUM;
    config.xclk_freq_hz     = 20000000;
    config.pixel_format     = PIXFORMAT_JPEG;
    config.frame_size       = FRAMESIZE_VGA;
    config.jpeg_quality     = 10;
    config.fb_count         = 1;

    if (esp_camera_init(&config) != ESP_OK) {
        DBG_PRINTLN("ERROR: Camera init failed");
        while(1);
    }
    else {
        DBG_PRINTLN("Camera initialised");
    }
}


// === initialise micro SD card ===
void initMicroSD() {
    DBG_PRINTLN("Initialising SD card...");

    // --- begin connection to micro SD card in 1-bit mode ---
    if (!SD_MMC.begin("/sdcard", true)) {
        DBG_PRINTLN("ERROR: Failed to initialise SD card");
        while(1);
    }
    else {
        DBG_PRINTLN("SD card initialised");
    }

    // --- check if SD card is inserted & readable ---
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        DBG_PRINTLN("ERROR: SD Card corrupted or not detected");
        while(1);
    }
}


// === initialise Wi-Fi ===
void initWifi() {
    // --- attempt connection if not connected ---
    if (WiFi.status() != WL_CONNECTED) {
        DBG_PRINT("Connecting to WiFi");

        // --- begin WIFI connection ---
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASS);

        // --- check connection status for 5 seconds ---
        unsigned long checkConnection_startTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - checkConnection_startTime < 5000) {
            delay(5);
            DBG_PRINT(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            DBG_PRINTLN("");
            DBG_PRINT("WiFi connected: ");
            DBG_PRINTLN(WiFi.localIP());
        } 
        else {
            DBG_PRINTLN("");
            DBG_PRINTLN("ERROR: WiFi connection failed");
            while(1);
        }
    }
}


// === sync with current local time ===
void initTime() {
    // --- configure time --- 
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // --- delay to wait for time configuration ---  
    delay(5000);

    // --- get local time ---
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        DBG_PRINTLN("ERROR: Failed to obtain time");
        while(1);
    }

    char timeNow[40];
    strftime(timeNow, sizeof(timeNow), "%Y-%m-%d_%H-%M-%S", &timeinfo);
    DBG_PRINTLN("Time synchronized via NTP @ " + String(timeNow));
}


// === blocking delay to stabilise PIR sensor readings ===
void initPIR() {
    unsigned long initPIR_startTime = millis();
    while (millis() - initPIR_startTime < initPIR_period) {
        mcp.digitalWrite(BLUE_LED_PIN, HIGH);
        delay(500);
        mcp.digitalWrite(BLUE_LED_PIN, LOW);
        delay(500);
    }

    // --- reset last action endtime to current time ---
    lastAction_endTime = millis();
}


// === ensure connection to MQTT  ===
void ensureMQTT() {
    // --- attempt to connect for 5 seconds ---
    unsigned long attemptConnection_startTime = millis();
    while (!mqtt.connected() && millis() - attemptConnection_startTime < 5000) {
        DBG_PRINTLN("Not connected to MQTT");
        delay(50);
        mqtt.connect(
            "smart-doorbell",
            MQTT_USER,
            MQTT_PASS
        );
    }

    if (!mqtt.connected()) {
        DBG_PRINTLN("ERROR: Failed to connect to MQTT");
        while(1);
    }
    else {
        DBG_PRINTLN("Connected to MQTT");
    }
}


// === get current time as a string ===
String getCurrentTimeString() {
    // --- get current time ---
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "UNKNOWN_TIME";
    }

    // --- cast & return timestamp as string ---
    char timeStamp[40];
    strftime(timeStamp, sizeof(timeStamp), "%Y-%m-%d_%H-%M-%S", &timeinfo);
    return String(timeStamp);
}


// === capture & save image ===
void captureAndSaveImage(String filename = getCurrentTimeString()) {
    
    // --- discard first frame ---
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);

    // --- delay for stability ---
    delay(50);
    
    // --- capture image as JPEG ---
    fb = esp_camera_fb_get();
    if (!fb) {
        DBG_PRINTLN("capture failed");
        while(1);
    }

    // --- set path of JPEG file ---
    String path = "/IMG_" + filename + ".jpg";
    Serial.printf("Picture file name: %s\n", path.c_str());

    // --- open file for writing ---
    fs::FS &fs = SD_MMC;
    File file = fs.open(path.c_str(), FILE_WRITE);

    // --- write captured frame to file as JPEG ---
    if (!file) {
        Serial.printf("Failed to open file in writing mode");
        while(1);
    } 
    else {
        file.write(fb->buf, fb->len);
        Serial.printf("Saved: %s\n", path.c_str());
    }

    /// close file & return frame buffer ---
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


// === ring if doorbell rung ===
void ringIfRung(unsigned long checkDuration = 100) {
    // --- check if active-low button pressed for checkDuration ---
    unsigned long checkDuration_startTime = millis();
    while (millis() - checkDuration_startTime < checkDuration) {

        // --- if button pushed & allowed time since last ring has passed ---
        if (mcp.digitalRead(BTN_PIN) == LOW && millis() - lastRing_endTime > timeSince_lastRing) {
            DBG_PRINTLN("Bell rung!");
            
            // --- connect to MQTT & notify MQTT that doorbell was rung ---
            ensureMQTT();
            mqtt.publish("doorbell/ring", "pressed");

            // --- capture & save image as last ring capture (overwrite) ---
            captureAndSaveImage(latestRingCapture_filename);

            // --- send this image to telegram ---
            sendImageToTelegram();

            // --- reset last ring endtime & last action endtime to current time ---
            lastRing_endTime = millis();
            lastAction_endTime = millis();
        }
    }
}


// === activate surveillance routine ===
void activateSurveillance() {
    mcp.digitalWrite(BUZZER_PIN, HIGH);

    // --- surveil for surveillance period ---
    surveillance_startTime = millis();
    while (millis() - surveillance_startTime <= surveillancePeriod) {
        mcp.digitalWrite(RED_LED_PIN, HIGH);

        // --- capture & save image to SD card every second ---
        captureAndSaveImage();
        mcp.digitalWrite(RED_LED_PIN, LOW);

        // --- check if rung ---
        ringIfRung();
        mcp.digitalWrite(BUZZER_PIN, LOW);
    }

    // --- reset last action endtime to current time ---
    lastAction_endTime = millis();
}


// === check if right time to upload ===
bool timeToUpload() {
    // --- get current time ---
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        DBG_PRINTLN("ERROR: Failed to obtain time");
        return false;
    }

    // --- extract the hour from the current time ---
    int currentHour = timeinfo.tm_hour;

    // --- check if the current time is between allowed upload hours ---
    if (currentHour >= 16 || currentHour < 4) {
        return true;
    } 
    else {
        return false;
    }
}


// === upload a JPEG file to cloudinary ===
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


// === upload & delete all images (JPEG files) from SD card ===
bool uploadAndDeleteAll() {
    // --- open SD root directory ---
    File root = SD_MMC.open("/");
    if (!root || !root.isDirectory()) {
        DBG_PRINTLN("ERROR: SD root open failed");
        lastAction_endTime = millis();
        return true;
    }

    // --- open next available file in root ---
    File file = root.openNextFile();

    // --- loop through files ---
    while (file) {
        delay(100);

        // --- skip folders, non-JPEGs & the latest ring capture ---
        String filename = file.name();
        if (file.isDirectory() || !filename.endsWith(".jpg") || filename == "IMG_" + latestRingCapture_filename + ".jpg") {
            file = root.openNextFile();
            continue;
        }

        // --- upload JPEG file to cloudinary ---
        bool ok = uploadImageToCloudinary(file, filename);

        // --- delete file if upload ok ---
        if (ok) {
            DBG_PRINTLN("Upload OK deleting " + filename + " from SD card");
            SD_MMC.remove("/" + filename);
        }
        else {
            DBG_PRINTLN("Upload failed, stopping uploads");
            lastAction_endTime = millis();
            return true;
        }

        // --- stop if motion detected ---
        if (mcp.digitalRead(PIR_PIN) == HIGH) {
            DBG_PRINTLN("Motion detected, stopping uploads");
            lastAction_endTime = millis();
            return true;
        }


        file = root.openNextFile();
        delay(100);
    }

    // --- reset last action endtime to current time & finish uploading ---
    DBG_PRINTLN("No images left to upload");
    lastAction_endTime = millis();
    return false;
}


// === initialize system & perform startup sequence ===
void setup() {
    DBG_DELAY(50);

    // --- record time at start of boot ---
    unsigned long boot_startTime = millis(); 

    // --- begin debug serial ---
    DBG_SERIAL_BEGIN(115200);

    // --- initialise sub-systems ---
    initMCP();
    initCamera();
    initMicroSD();
    initWifi();

    // --- set MQTT server ---
    mqtt.setServer(MQTT_HOST, MQTT_PORT);

    // --- set pinmodes ---
    mcp.pinMode(BLUE_LED_PIN, OUTPUT);
    mcp.pinMode(RED_LED_PIN, OUTPUT);
    mcp.pinMode(BUZZER_PIN, OUTPUT);
    mcp.pinMode(BTN_PIN, INPUT_PULLUP);
    mcp.pinMode(PIR_PIN, INPUT);
    pinMode(WAKE_PIN, INPUT_PULLDOWN);

    // --- set initial states of peripherals ---
    mcp.digitalWrite(BLUE_LED_PIN, HIGH);
    mcp.digitalWrite(RED_LED_PIN, LOW);
    mcp.digitalWrite(BUZZER_PIN, LOW);

    // --- configure pin as a source to wake when goes HIGH ---
    esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_PIN, 1);

    // --- get reason for wake ---
    esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();

    // --- check wake reason ---
    switch (wakeupReason) {

        // --- sync time & initialise PIR if woke from power on ---
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            DBG_PRINTLN("Cold boot");
            initTime();
            initPIR();
            break;

        // --- activate surveillance immediately if woke from wake source ---
        case ESP_SLEEP_WAKEUP_EXT0:
            DBG_PRINTLN("Wakeup by PIR");
            activateSurveillance();
            break;

        // --- if unknown wake reason ---
        default:
            DBG_PRINTLN("Unknown wake reason");
            while(1);
            break;
    }

    DBG_PRINT("Boot duration: ");
    DBG_PRINTLN(millis() - boot_startTime);

    DBG_PRINTLN("Runtime begin");
}


// === main runtime loop ===
void loop() {
    // --- keep MQTT running ---
    mqtt.loop();

    // --- set default runtime states of peripherals ---
    mcp.digitalWrite(BLUE_LED_PIN, HIGH);
    mcp.digitalWrite(RED_LED_PIN, LOW);
    mcp.digitalWrite(BUZZER_PIN, LOW);

    // --- ring if doorbell rung ---
    ringIfRung();

    // --- activate surveillance if PIR input HIGH (motion detected) ---
    if (mcp.digitalRead(PIR_PIN) == HIGH) {
        DBG_PRINTLN("Motion detected");
        activateSurveillance();
    }
    // --- if images left to upload on SD card & right time to upload ---
    else if (imagesLeftToUpload == true && timeToUpload() == true) {
        // --- upload all images to cloudinary and delete from SD card ---
        imagesLeftToUpload = uploadAndDeleteAll();
    }
    // --- if maximum allowed standby duration has passed ---
    else if (millis() - lastAction_endTime >= allowedStandbyDuration) {
        DBG_PRINTLN("ESP32-CAM entering deep sleep");
        DBG_DELAY(1000);

        // --- hold led in current state (HIGH) ---
        gpio_hold_en((gpio_num_t)BLUE_LED_PIN);

        // --- deinitialise camera ---
        esp_camera_deinit();

        // --- enter deepsleep ---
        esp_deep_sleep_start();
    }
}
