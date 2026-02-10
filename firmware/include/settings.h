#pragma once
#include <Arduino.h>


// === firmware version ===
extern const String FW_VERSION;


// === time settings ===
extern const char* ntpServer;
extern const long  gmtOffset_sec;
extern const int   daylightOffset_sec;


// === filename of image captured at latest doorbell ring ===
extern const String latestRingCapture_filename;


// === telegram ===
// --- host url ---
extern const char* telegramHost;
// --- caption sent to telegram with latest ring capture ---
extern String captionText;


// === cloudinary ===
// --- host url ---
extern const char* cloudinaryHost;


// === upload window ===
extern const int UPLOAD_START_HOUR;
extern const int UPLOAD_END_HOUR;


// === to check if any images left to upload on SD card ===
extern bool imagesLeftToUpload;


// === time variables ===
extern unsigned long initPIR_startTime;             // time at start of PIR initialsation
extern unsigned long lastAction_endTime;            // time when last action ended
extern unsigned long lastRing_endTime; 
extern unsigned long surveillance_startTime;        // time at start of surveillance

extern const unsigned long surveillancePeriod;      // allowed suveillance duration
extern const unsigned long initPIR_period;          // duration of initialising PIR sensor
extern const unsigned long allowedStandbyDuration;  // maximum allowed standby duration
extern const unsigned long timeSince_lastRing; 