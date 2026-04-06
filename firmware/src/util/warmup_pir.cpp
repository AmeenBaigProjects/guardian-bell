// === project headers ===
// --- corresponding header ---
#include "warmup_pir.h"

// --- configuration ---
#include "settings.h"
#include "pins.h"

// --- hardware ---
#include "mcp23017.h"

// --- util ---
#include "wipe_sd_card.h"


/// === blocking delay to warm up PIR sensor & get stable readings ===
void warmUpPIR() {

    /// --- time at start of PIR warmup ---
    unsigned long startMs = millis();

    /// --- delay with led blink visual indication ---
    while (millis() - startMs < warmUpPeriod) {
        mcp.digitalWrite(BLUE_LED_PIN, HIGH);
        deleteAll();
        delay(500);
        mcp.digitalWrite(BLUE_LED_PIN, LOW);
        delay(500);
    }

    /// --- reset last action endtime to current time ---
    lastActionTime = millis();
}