// === standard headers ===
// --- HTTP client for REST requests / file download ---
#include <HTTPClient.h>

// --- TLS/SSL client for secure HTTPS connections
#include <WiFiClientSecure.h>

// --- OTA firmware update handling ---
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


/// === WIFI client setup ===
WiFiClientSecure otaClient;


/// === fetch the latest version of the remote firmware ===
static String fetchRemoteFirmwareVersion() {
    /// --- skip certificate validation ---
    otaClient.setInsecure();
    
    HTTPClient http;
    http.begin(otaClient, OTA_VERSION_URL);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int code = http.GET();

    DBG_PRINT("HTTP code (version): "); 
    DBG_PRINTLN(code);

    if (code != HTTP_CODE_OK) {
        DBG_PRINT("OTA version fetch failed with HTTP code: ");
        DBG_PRINTLN(code);
        error("OTA firmware version fetch failed (HTTP " + String(code) + ")", false);
        http.end();
        return "";
    }

    String version = http.getString();
    version.trim();

    http.end();

    return version;
}


/// === fetch update notes for the latest version of the remote firmware ===
static String fetchUpdateNotes() {
    /// --- skip certificate validation ---
    otaClient.setInsecure();

    HTTPClient http;
    http.begin(otaClient, OTA_UPDATE_NOTES_URL);

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int code = http.GET();

    DBG_PRINT("HTTP code (notes): ");
    DBG_PRINTLN(code);

    if (code != HTTP_CODE_OK) {
        DBG_PRINT("OTA update notes fetch failed with HTTP code: ");
        DBG_PRINTLN(code);
        error("OTA firmware update notes fetch failed (HTTP " + String(code) + ")", false);
        http.end();
        return "";
    }

    String notes = http.getString();
    notes.trim();

    http.end();

    return notes;
}


/// === flash firmware OTA ===
/// returns true on success, false on failure
bool performFirmwareUpdateOTA(String rmtVersion) {
    /// --- get update notes ---
    String updateNotes = fetchUpdateNotes();

    /// --- skip certificate validation ---
    otaClient.setInsecure();
    
    HTTPClient http;
    http.begin(otaClient, OTA_FIRMWARE_URL);

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        DBG_PRINT("OTA firmware download failed with HTTP code: ");
        DBG_PRINTLN(code);
        error("OTA firmware download failed (HTTP " + String(code) + ")", false);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    
    if (contentLength <= 0) {
        DBG_PRINT("OTA invalid content length: ");
        DBG_PRINTLN(contentLength);
        error("OTA invalid content length (" + String(contentLength) + ")", false);
        http.end();
        return false;
    }

    if (!Update.begin(contentLength)) {
        DBG_PRINTLN("OTA firmware update could not begin");
        error("OTA firmware update start failed", false);
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);

    if (written != (size_t)contentLength) {
        DBG_PRINT("OTA incomplete write: ");
        DBG_PRINT(written);
        DBG_PRINT(" / ");
        DBG_PRINTLN(contentLength);
        error("OTA firmware incomplete write (" + String(written) + "/" + String(contentLength) + " bytes)", false);
        Update.abort();
        http.end();
        return false;
    }

    if (!Update.end()) {
        DBG_PRINTLN("OTA firmware update end failed");
        error("OTA firmware update end failed", false);
        http.end();
        return false;
    }

    if (!Update.isFinished()) {
        DBG_PRINTLN("OTA firmware update not finished");
        error("OTA firmware update not finished", false);
        http.end();
        return false;
    }

    /// --- notify firmware update success via telegram ---
    sendMsgToTelegram("Firmware updated successfully from " + FW_VERSION + " to " + rmtVersion);

    delay(1000);

    /// --- send firmware update notes via telegram ---
    sendMsgToTelegram("GuardianBell " + rmtVersion + ":\n" + updateNotes);

    delay(2000);
    
    http.end();

    ESP.restart();

    return true;
}


/// === check for firmware update with retry logic ===
void checkForFirmwareUpdate() {
    DBG_PRINTLN("Checking for firmware update");

    /// --- get remote firmware version ---
    String remoteVersion = fetchRemoteFirmwareVersion();
    if (remoteVersion.length() == 0) {
        error("No remote firmware version available", false);
        return;
    }

    DBG_PRINT("Local firmware version = ");
    DBG_PRINTLN(FW_VERSION);
    DBG_PRINT("Remote firmware version = ");
    DBG_PRINTLN(remoteVersion);

    /// --- compare local firmware version to remote firmware version ---
    if (remoteVersion == FW_VERSION) {
        DBG_PRINTLN("Firmware up to date");
        return;
    }

    DBG_PRINTLN("Firmware update available");

    /// --- attempt firmware update with retries ---
    for (int attempt = 1; attempt <= OTA_MAX_RETRIES; attempt++) {
        DBG_PRINT("OTA update attempt ");
        DBG_PRINT(attempt);
        DBG_PRINT(" of ");
        DBG_PRINTLN(OTA_MAX_RETRIES);

        sendMsgToTelegram("OTA update attempt " + String(attempt) + "/" + String(OTA_MAX_RETRIES) + ": updating to " + remoteVersion);

        bool success = performFirmwareUpdateOTA(remoteVersion);

        /// --- if update succeeded, performFirmwareUpdateOTA restarts the device ---
        /// --- reaching here means the update failed ---
        if (!success) {
            DBG_PRINT("OTA update attempt ");
            DBG_PRINT(attempt);
            DBG_PRINTLN(" failed");

            if (attempt < OTA_MAX_RETRIES) {
                DBG_PRINT("Retrying in ");
                DBG_PRINT(OTA_RETRY_DELAY_MS / 1000);
                DBG_PRINTLN(" seconds...");
                delay(OTA_RETRY_DELAY_MS);
            }
        }
    }

    /// --- all retry attempts exhausted ---
    DBG_PRINTLN("OTA update failed after all retry attempts");
    error("OTA update failed after " + String(OTA_MAX_RETRIES) + " attempts. Target version: " + remoteVersion, false);
}
