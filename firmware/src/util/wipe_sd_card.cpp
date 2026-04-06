// === standard headers ===
// --- SD card access via SD_MMC interface ---
#include <SD_MMC.h>


// === project headers ===
// --- corresponding header ---
#include "wipe_sd_card.h"

// --- configuration ---
#include "settings.h"
#include "pins.h"

// --- hardware ---
#include "mcp23017.h"

// --- utilities ---
#include "debug.h"
#include "error.h"
#include "button_interrupt.h"


/// === delete all images (JPEG files) from SD card ===
void deleteAll() {
    bool shouldDelete = false;

    /// --- poll the MCP button pin if serial debugging ON ---
    #if SERIAL_DEBUG
        /// --- check state of active-low button ---
        static bool lastState = HIGH;
        bool currentState = mcp.digitalRead(BTN_MCP_PIN);

        /// --- if button pushed prepare to delete ---
        if (lastState == HIGH && currentState == LOW) {
            shouldRing = true;
        }

        lastState = currentState;

    /// --- else check the interrupt flag set by the ESP button pin ---
    #else
        if (doorbellInterrupted) {
            shouldDelete = true;
        }
    #endif

    if (shouldDelete) {
        /// --- sound buzzer ---
        mcp.digitalWrite(BUZZER_PIN, HIGH);
        delay(200);
        mcp.digitalWrite(BUZZER_PIN, LOW);
        delay(200);
        mcp.digitalWrite(BUZZER_PIN, HIGH);
        delay(200);
        mcp.digitalWrite(BUZZER_PIN, LOW);

        /// --- reset the interrupt flag ---
        doorbellInterrupted = false;

        /// --- open SD root directory ---
        File root = SD_MMC.open("/");
        if (!root || !root.isDirectory()) {
            error("ERROR: SD root open failed", false);
        }

        /// --- open next available file in root ---
        File file = root.openNextFile();

        /// --- loop through files ---
        while (file) {
            delay(50);
            mcp.digitalWrite(BLUE_LED_PIN, HIGH);

            /// --- skip folders, non-JPEGs & the latest ring capture ---
            String filename = file.name();
            if (file.isDirectory() || !filename.endsWith(".jpg") || filename == "IMG_" + lastRingCaptureFilename + ".jpg") {
                file = root.openNextFile();
                continue;
            }

            /// --- delete file ---
            DBG_PRINTLN("Upload OK deleting " + filename + " from SD card");
            SD_MMC.remove("/" + filename);

            /// --- open next available file in root ---
            file = root.openNextFile();

            mcp.digitalWrite(BLUE_LED_PIN, LOW);
        }
    }

}
