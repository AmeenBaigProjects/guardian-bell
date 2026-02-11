// === standard headers ===
#include <time.h>
#include <esp_sleep.h>


// === project headers ===
// --- corresponding header ---
#include "time_util.h"

// --- cofiguration ---
#include "settings.h"

// --- utilities ---
#include "debug.h"
#include "error.h"


// === sync with current local time ===
void initTime() {
    // --- configure time --- 
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // --- delay to wait for time configuration ---  
    delay(1000);
    DBG_PRINTLN("Tik");
    delay(1000);
    DBG_PRINTLN("Tok");
    delay(1000);
    DBG_PRINTLN("Tik");
    delay(1000);
    DBG_PRINTLN("Tok");
    delay(1000);


    // --- get local time ---
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        error("Failed to obtain time", false);
    }

    // --- debug: print local time ---
    char timeNow[40];
    strftime(timeNow, sizeof(timeNow), "%Y-%m-%d_%H-%M-%S", &timeinfo);
    DBG_PRINTLN("Time synchronized via NTP @ " + String(timeNow));
}


// === get current date & time as a string ===
String getCurrentDateTime() {
    // --- get current time ---
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "UNKNOWN_TIME";
    }

    // --- cast & return timestamp as string ---
    char timeStamp[40];
    strftime(timeStamp, sizeof(timeStamp), "%Y-%m-%d_%H-%M-%S", &timeinfo);
    return String(timeStamp);
}


// === schedule a random wake-up time for overnight cloud upload ===
void scheduleRandomTimerWake() {

    // --- get current local time ---
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        error("Cannot schedule upload wake (no time)", false);
        return;
    }

    // --- convert current time to epoch seconds ---
    time_t now = mktime(&timeinfo);

    // --- build start of today's upload window ---
    struct tm uploadStart = timeinfo;
    uploadStart.tm_hour = UPLOAD_START_HOUR;
    uploadStart.tm_min  = 0;
    uploadStart.tm_sec  = 0;
    time_t uploadStartTs = mktime(&uploadStart);

    // --- build end of today's upload window ---
    struct tm uploadEnd = timeinfo;
    uploadEnd.tm_hour = UPLOAD_END_HOUR;
    uploadEnd.tm_min  = 0;
    uploadEnd.tm_sec  = 0;
    time_t uploadEndTs = mktime(&uploadEnd);

    // --- handle window spanning midnight ---
    if (UPLOAD_START_HOUR > UPLOAD_END_HOUR) {
        // window spans midnight (e.g. 16 → 04)
        if (now < uploadEndTs) {
            // after midnight, before end
            uploadStartTs -= 24 * 3600;
        } else {
            // before midnight, same day start
            uploadEndTs += 24 * 3600;
        }
    }

    // --- if currently inside upload window, start from NOW ---
    if (now >= uploadStartTs && now < uploadEndTs) {
        uploadStartTs = now;
    }
    // --- otherwise, if already past today's window, move to tomorrow ---
    else if (now >= uploadEndTs) {
        uploadStartTs += 24 * 3600;
        uploadEndTs   += 24 * 3600;
    }

    // --- calculate remaining window length ---
    uint32_t windowSeconds = difftime(uploadEndTs, uploadStartTs);

    if (windowSeconds == 0) {
        error("Upload window length is zero", false);
        return;
    }

    // --- generate random offset inside window ---
    uint32_t randomOffset = esp_random() % windowSeconds;

    // --- final scheduled wake-up time ---
    time_t wakeTime = uploadStartTs + randomOffset;

    // --- compute sleep duration from now ---
    uint64_t sleepSeconds = difftime(wakeTime, now);

    // --- debug output ---
    DBG_PRINT("Scheduling upload wake in ");
    DBG_PRINT(sleepSeconds);
    DBG_PRINTLN(" seconds");

    // --- configure deep sleep timer wake-up ---
    esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
}



// === check if right time to upload ===
bool timeToUpload() {
    // --- get current time ---
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        error("Failed to obtain time", false);
        return false;
    }

    // --- extract the hour from the current time ---
    int currentHour = timeinfo.tm_hour;

    // --- check if the current time is between allowed upload hours ---
    if (UPLOAD_START_HOUR > UPLOAD_END_HOUR) {
        return (currentHour >= UPLOAD_START_HOUR || currentHour < UPLOAD_END_HOUR);
    } 
    else {
        return (currentHour >= UPLOAD_START_HOUR && currentHour < UPLOAD_END_HOUR);
    }
}