// === standard headers ===
// --- Arduino core functions & types ---
#include <Arduino.h>

// --- WiFi connectivity ---
#include "WiFi.h"

// --- I2C communication library ---
#include <Wire.h>

// --- ESP32-CAM driver ---
#include <esp_camera.h>

// --- SD card access via SD_MMC interface ---
#include <SD_MMC.h>

// --- ESP32 sleep modes ---
#include <esp_sleep.h>

// --- system time functions ---
#include <time.h>

// --- HTTP client for REST requests / file download ---
#include <HTTPClient.h>

// --- TLS/SSL client for secure HTTPS connections ---
#include <WiFiClientSecure.h>

// --- MQTT client library ---
#include <PubSubClient.h> 

// --- OTA firmware update handling (flash writing) ---
#include <Update.h>

// --- access system control ---
#include <soc/soc.h>

// --- access RTC control registers ---
#include <soc/rtc_cntl_reg.h>


// === project headers ===
// --- secrets_example.h for reference ---
#include "secrets.h"

// --- configuration ---
#include "settings.h"
#include "pins.h"

// --- hardware ---
#include "camera.h"
#include "mcp23017.h"
#include "microSD_card.h"

// --- network ---
#include "wifi.h"
#include "mqtt.h"

// --- services ---
#include "telegram.h"
#include "cloudinary.h"
#include "ota.h"
 
// --- utilities ---
#include "debug.h"
#include "error.h"
#include "time_util.h"
#include "security_alarm.h"
#include "warmup_pir.h"
#include "capture_save_image.h"


// === global variables with default values set ===
// --- time at end of last action ---
unsigned long lastAction_endTime    = 0;

// --- time at end of last ring ---
unsigned long lastRing_endTime      = 0;

// --- number of motion detections ---
int motionDectctionCount = 0;

// --- to check if warned once ---
bool warnedOnce = false;

// --- to check if warned twice ---
bool warnedTwice = false;


// === ring ===
// --- ring if doorbell rung ---
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


// === surveil ===
// --- activate surveillance routine ---
void activateSurveillance() {

    // --- time at start of surveillance ---
    unsigned long  startMs = millis();

    // --- surveil for surveillance period ---
    while (millis() - startMs <= surveillancePeriod) {
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


// === upload ===
// --- upload & delete all images (JPEG files) from SD card ---
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

        // --- open next available file in root ---
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

    // --- disable brownout detector ---
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // --- record time at start of boot ---
    unsigned long boot_startTime = millis(); 

    // --- begin debug serial ---
    DBG_SERIAL_BEGIN(115200);

    // --- initialise hardware ---
    initMCP();
    initCamera();
    initMicroSD();

    // --- connect to WiFi ---
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

    //error("For testing (not an error)");

    // --- configure pin as a source to wake when goes HIGH ---
    esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_PIN, 1);

    // --- get reason for wake ---
    esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();

    // --- check wake reason ---
    switch (wakeupReason) {

        // --- sync time & initialise PIR if woke from power on ---
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            DBG_PRINTLN("Cold boot");
            checkForFirmwareUpdate();
            initTime();
            warmUpPIR();
            break;

        // --- activate surveillance immediately if woke from wake source ---
        case ESP_SLEEP_WAKEUP_EXT0:
            DBG_PRINTLN("Wakeup by PIR");
            mcp.digitalWrite(BUZZER_PIN, HIGH);
            activateSurveillance();
            motionDectctionCount++;
            break;

        // --- if woke from timer wake ---
        case ESP_SLEEP_WAKEUP_TIMER:
            DBG_PRINTLN("Timer wake");
            initTime();
            break;

        // --- if unknown wake reason ---
        default:
            error("Unknown wake reason", false);
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

    // --- notify user if motion detections exceed suspicious activity threshold ---
    if (motionDectctionCount > acceptableDetections && !warnedOnce) {
        sendMsgToTelegram("⚠️ Suspicious activity near your door!");
        warnedOnce = true;
    }

    // --- sound alarm if motion detections are seriously high ---
    if (motionDectctionCount > 2*acceptableDetections && !warnedTwice) {
        soundAlarm(60000);
        warnedTwice = true;
    }

    // --- ring if doorbell rung ---
    ringIfRung();

    // --- activate surveillance if PIR input HIGH (motion detected) ---
    if (mcp.digitalRead(PIR_PIN) == HIGH) {
        DBG_PRINTLN("Motion detected");
        activateSurveillance();
        motionDectctionCount++;
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

        // --- shedule next random time to upload if images left to upload ---
        if (imagesLeftToUpload) {
            scheduleRandomTimerWake();
        }

        // --- hold led in current state (HIGH) ---
        gpio_hold_en((gpio_num_t)BLUE_LED_PIN);

        // --- deinitialise camera ---
        esp_camera_deinit();

        // --- enter deepsleep ---
        esp_deep_sleep_start();
    }
}