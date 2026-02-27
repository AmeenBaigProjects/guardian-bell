// === standard headers ===
// --- ESP32-CAM driver ---
#include <esp_camera.h>

// --- SD card access via SD_MMC interface ---
#include <SD_MMC.h>

// === project headers ===
// --- corresponding header ---
#include "capture_save_image.h"

// --- configuration ---
#include "settings.h"
#include "pins.h"

// --- hardware ---
#include "camera.h"
#include "microSD_card.h"

// --- utilities ---
#include "debug.h"
#include "error.h"


// === capture & save image to SD card===
void captureAndSaveImage(String filename) {
    
    // --- discard first frame ---
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);

    // --- delay for stability ---
    delay(50);
    
    // --- capture image as JPEG ---
    fb = esp_camera_fb_get();
    if (!fb) {
        DBG_PRINTLN("capture failed");
        error("Capture failed", true);
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
        // error("Failed to open file in writing mode");
    } 
    else {
        file.write(fb->buf, fb->len);
        Serial.printf("Saved: %s\n", path.c_str());
    }

    // --- close file & return frame buffer ---
    file.close();
    esp_camera_fb_return(fb);
}