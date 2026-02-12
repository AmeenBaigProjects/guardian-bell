// === standard headers ===
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>


// === project headers ===
// --- corresponding header ---
#include "ota.h"

// --- secrets_example.h for reference ---
#include "secrets.h"

// --- configuration ---
#include "settings.h"

// --- network ---
#include "wifi.h"

// --- services ---
#include "telegram.h"

// --- utilities ---
#include "debug.h"
#include "error.h"


// === WIFI client setup ===
WiFiClientSecure otaClient;


// === fetch the latest version of the remote firmware ===
static String fetchRemoteFirmwareVersion() {
    // --- skip certificate validation ---
    otaClient.setInsecure();
    
    HTTPClient http;
    http.begin(otaClient, OTA_VERSION_URL);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        error("OTA firmware version fetch failed", false);
        http.end();
        return "";
    }

    String version = http.getString();
    version.trim();

    http.end();
    return version;
}

// === flash firmware OTA ===
void performFirmwareUpdateOTA(String rmtVersion) {
    // --- skip certificate validation ---
    otaClient.setInsecure();
    
    HTTPClient http;
    http.begin(otaClient, OTA_VERSION_URL);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        error("OTA firmware download failed", false);
        http.end();
    }

    int contentLength = http.getSize();
    
    if (contentLength <= 0) {
        error("OTA invalid content length", false);
        http.end();
    }
    else if (!Update.begin(contentLength)) {
        error("OTA firmware update start failed", false);
        http.end();
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);

    if (written != contentLength) {
        error("OTA firmware incomplete write", false);
        Update.abort();
        http.end();
    }
    else if (!Update.end()) {
        error("OTA firmware update end failed", false);
        http.end();
    }
    else if (!Update.isFinished()) {
        error("OTA firmware update not finished", false);
        http.end();
    }

    // --- notify firmware update sucess via telegram ---
    sendMsgToTelegram("Firmware updated sucessfully from " + FW_VERSION + " to " + rmtVersion);
    
    http.end();

    delay(500);

    ESP.restart();
}


// === check for firmware update ===
void checkForFirmwareUpdate() {
    DBG_PRINTLN("Checking for firmware update");

    // --- get remote firmware version ---
    String remoteVersion = fetchRemoteFirmwareVersion();
    if (remoteVersion.length() == 0) {
        error("No remote firmware version available", false);
        return;
    }

    DBG_PRINT("Local firmware version = ");
    DBG_PRINTLN(FW_VERSION);
    DBG_PRINT("Remote firmware version = ");
    DBG_PRINTLN(remoteVersion);

    // --- compare local firmware version to remote firmware version ---
    if (remoteVersion == FW_VERSION) {
        DBG_PRINTLN("Firmware up to date");
        return;
    }
    else {
        DBG_PRINTLN("Firmware update available");
        performFirmwareUpdateOTA(remoteVersion);
    }
}
