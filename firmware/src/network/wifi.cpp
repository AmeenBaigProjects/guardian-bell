// === standard headers ===
#include "WiFi.h"


// === project headers ===
// --- corresponding header ---
#include "wifi.h"

// --- secrets_example.h for reference ---
#include "secrets.h"

// --- utilities ---
#include "debug.h"
#include "error.h"


// === initialise Wi-Fi ===
void initWifi() {
    // --- attempt connection if not connected ---
    if (WiFi.status() != WL_CONNECTED) {
        DBG_PRINT("Connecting to WiFi");

        // --- begin WIFI connection ---
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASS);

        // --- check connection status for 5 seconds ---
        unsigned long checkConnection_startTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - checkConnection_startTime < 5000) {
            delay(5);
            DBG_PRINT(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            DBG_PRINTLN("");
            DBG_PRINT("WiFi connected: ");
            DBG_PRINTLN(WiFi.localIP());
        } 
        else {
            DBG_PRINTLN("");
            error("WiFi connection failed");
        }
    }
}
