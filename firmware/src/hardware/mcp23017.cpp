// === standard headers ===
#include <Wire.h>


// === project headers ===
// --- corresponding header ---
#include "mcp23017.h"

// --- secrets_example.h for reference ---
#include "secrets.h"

// --- configuration ---
#include "settings.h"
#include "pins.h"

// --- network ---
#include "wifi.h"

// --- utilities ---
#include "debug.h"
#include "error.h"


// === MCP23017 configuration ===
Adafruit_MCP23X17 mcp;


// === initialiase MCP23017 as a GPIO expander ===
void initMCP(){
    DBG_PRINTLN("Initialising MCP23017...");
    Wire.begin(SDA_PIN, SCL_PIN);

    if (!mcp.begin_I2C(0x20, &Wire)) {
        error("Failed to initialise MCP23017", true);
    }
    else {
        DBG_PRINTLN("Initialised MCP23017");
    }
}