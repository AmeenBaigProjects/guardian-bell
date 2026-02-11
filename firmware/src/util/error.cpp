// === project headers ===
// --- corresponding header ---
#include "error.h"

// --- configuration ---
#include "pins.h"

// --- services ---
#include "telegram.h"

// --- utilities ---
#include "debug.h"


// === error handler ===
void error(const String& message, bool fatalError) {
    // --- when error is fatal ---
    if(fatalError) {
        DBG_PRINT("FATAL ERROR: ");
        DBG_PRINTLN(message);

        // --- attempt Telegram notification ---
        sendMsgToTelegram("FATAL ERROR" + message);

        // --- infinite blink visual indicator ---
        pinMode(FLASH_LED_PIN, OUTPUT);
        while(1) {
            digitalWrite(FLASH_LED_PIN, HIGH);
            delay(200);
            digitalWrite(FLASH_LED_PIN, LOW);
            delay(200);
        }
    }
    // --- when error is non-fatal ---
    else {
        DBG_PRINT("ERROR: ");
        DBG_PRINTLN(message);

        // --- attempt Telegram notification ---
        sendMsgToTelegram("ERROR" + message);
    }
}