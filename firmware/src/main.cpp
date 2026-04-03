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
#include "button_interrupt.h"
#include "wipe_sd_card.h"


// === global variables with default values set ===
// -- time at end of last interrupt --
unsigned long lastInterruptTime = 0; 

// --- time at end of last action ---
unsigned long lastActionTime    = 0;

// --- time at end of last ring ---
unsigned long lastRingTime      = 0;

// --- number of motion detections ---
int motionDectctionCount = 0;

// --- to check if warned once ---
bool warnedOnce = false;

// --- to check if warned twice ---
bool warnedTwice = false;


// === ring ===
// --- ring if doorbell rung ---
void ringIfRung() {
    bool shouldRing = false;

    // --- poll the MCP button pin if serial debugging ON ---
    #if SERIAL_DEBUG
        // --- check state of active-low button ---
        static bool lastState = HIGH;
        bool currentState = mcp.digitalRead(BTN_MCP_PIN);

        // --- if button pushed prepare to ring ---
        if (lastState == HIGH && currentState == LOW) {
            shouldRing = true;
        }

        lastState = currentState;

    // --- else check the interrupt flag set by the ESP button pin ---
    #else
        if (doorbellInterrupted) {
            shouldRing = true;
        }
    #endif

    if (shouldRing) {

        // --- reset the interrupt flag ---
        doorbellInterrupted = false;

        // --- if button pushed & allowed time since last ring has passed ---
        if (millis() - lastRingTime > timeSinceLastRing) {
            DBG_PRINTLN("Bell rung!");

            // --- capture & save image as last ring capture ---
            captureAndSaveImage(lastRingCaptureFilename);
            
            // --- connect to MQTT & notify MQTT that doorbell was rung ---
            ensureMQTT();
            mqtt.publish("doorbell/ring", "pressed");

            // --- send this image to telegram ---
            sendImageToTelegram(captionText);

            // --- reset last ring endtime & last action endtime to current time ---
            lastRingTime = millis();
            lastActionTime = millis();
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
        captureAndSaveImage(getCurrentDateTime());
        mcp.digitalWrite(RED_LED_PIN, LOW);

        // --- check if rung ---
        ringIfRung();
    }

    // --- reset last action endtime to current time ---
    lastActionTime = millis();
}


// === upload ===
// --- upload & delete all images (JPEG files) from SD card ---
bool uploadAndDeleteAll() {

    // --- open SD root directory ---
    File root = SD_MMC.open("/");
    if (!root || !root.isDirectory()) {
        DBG_PRINTLN("ERROR: SD root open failed");
        lastActionTime = millis();
        return true;
    }

    // --- open next available file in root ---
    File file = root.openNextFile();

    // --- loop through files ---
    while (file) {
        delay(100);

        // --- skip folders, non-JPEGs & the last ring capture ---
        String filename = file.name();
        if (file.isDirectory() || !filename.endsWith(".jpg") || filename == "IMG_" + lastRingCaptureFilename + ".jpg") {
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
            lastActionTime = millis();
            return true;
        }

        // --- stop if motion detected ---
        if (mcp.digitalRead(PIR_PIN) == HIGH) {
            DBG_PRINTLN("Motion detected, stopping uploads");
            lastActionTime = millis();
            return true;
        }

        // --- open next available file in root ---
        file = root.openNextFile();
        delay(100);
    }

    // --- reset last action endtime to current time & finish uploading ---
    DBG_PRINTLN("No images left to upload");
    lastActionTime = millis();
    return false;
}


// === initialize system & perform startup sequence ===
void setup() {
    DBG_DELAY(50);

    // --- disable brownout detector ---
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // --- record time at start of boot ---
    unsigned long boot_startTime = millis(); 

    // --- initialise MCP23017 as GPIO expander ---
    initMCP();

    // --- if serial debugging begin debug serial & set button pinmode on the MCP23017 ---
    #if SERIAL_DEBUG
        Serial.begin(115200);

        mcp.pinMode(BTN_MCP_PIN, INPUT_PULLUP);

    // --- else set button pinmode on the ESP32-CAM ---
    #else
        pinMode(BTN_ESP_PIN, INPUT_PULLUP); 
    
        attachInterrupt(digitalPinToInterrupt(BTN_ESP_PIN), handleButtonInterrupt, FALLING);
    #endif

    // --- initialise camera and micro SD card ---
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
            mcp.digitalWrite(BUZZER_PIN, HIGH);
            delay(300);
            mcp.digitalWrite(BUZZER_PIN, LOW);
            DBG_PRINTLN("Cold boot");
            checkForFirmwareUpdate();
            initTime();
            warmUpPIR();
            break;

        // --- activate surveillance immediately if woke from wake source ---
        case ESP_SLEEP_WAKEUP_EXT0:
            DBG_PRINTLN("Wakeup by PIR");
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
        
        // --- capture & save image ---
        captureAndSaveImage(lastRingCaptureFilename);

        // --- send this image to telegram ---
        sendImageToTelegram("⚠️ Suspicious activity near your door!");

        warnedOnce = true;
    }

    // --- sound alarm if motion detections are seriously high ---
    if (motionDectctionCount > 2*acceptableDetections && !warnedTwice) {
                
        // --- capture & save image ---
        captureAndSaveImage(lastRingCaptureFilename);

        // --- send this image to telegram ---
        sendImageToTelegram("⚠️ Seriously suspicious activity near your door!");

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
    else if (millis() - lastActionTime >= allowedStandbyDuration) {
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