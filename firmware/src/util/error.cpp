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
void error(const String& message, bool blinkInfinitely) {
    DBG_PRINT("ERROR: ");
    DBG_PRINTLN(message);

    // --- attempt Telegram notification (best-effort) ---
    sendMsgToTelegram(message);

    // --- visual indicator (never exits) ---
    if (blinkInfinitely) {
        pinMode(FLASH_LED_PIN, OUTPUT);
        while(1) {
            digitalWrite(FLASH_LED_PIN, HIGH);
            delay(200);
            digitalWrite(FLASH_LED_PIN, LOW);
            delay(200);
        }
    }
}