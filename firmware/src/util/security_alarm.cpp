// === standard headers ===
// --- I2C communication library ---
#include <Wire.h>

// === project headers ===
// --- corresponding header ---
#include "security_alarm.h"

// --- configuration ---
#include "pins.h"

// --- hardware ---
#include "mcp23017.h"


// === sound alarm using buzzer ===
void soundAlarm(unsigned long durationMs){
    unsigned long startMs = millis();

    while (millis() - startMs < durationMs) {
        mcp.digitalWrite(BUZZER_PIN, HIGH);
        delay(500);
        mcp.digitalWrite(BUZZER_PIN, LOW);
        delay(500);
    }
}
