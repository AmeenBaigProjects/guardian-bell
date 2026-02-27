// === project headers ===
// --- corresponding header ---
#include "warmup_pir.h"

// --- configuration ---
#include "settings.h"
#include "pins.h"

// --- hardware ---
#include "mcp23017.h"


// === blocking delay to warm up PIR sensor & get stable readings ===
void warmUpPIR() {

    // --- time at start of PIR warmup ---
    unsigned long startMs = millis();

    // --- delay with led blink visual indication ---
    while (millis() - startMs < warmUpPIR_period) {
        mcp.digitalWrite(BLUE_LED_PIN, HIGH);
        delay(500);
        mcp.digitalWrite(BLUE_LED_PIN, LOW);
        delay(500);
    }

    // --- reset last action endtime to current time ---
    lastAction_endTime = millis();
}