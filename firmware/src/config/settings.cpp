// === project headers ===
// --- corresponding header ---
#include "settings.h"


// === firmware version ===
const String FW_VERSION = "0.1";


// === time settings ===
const char* ntpServer = "uk.pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;


// === filename of image captured at latest doorbell ring ===
const String latestRingCapture_filename = "latest_ring_capture";


// === telegram ===
// --- host url ---
const char* telegramHost = "api.telegram.org"; 
// --- caption sent to telegram with latest ring capture ---
String captionText = "🔔 Someone's at the door!";


// === cloudinary ===
// --- host url ---
const char* cloudinaryHost = "api.cloudinary.com";


// === upload window ===
const int UPLOAD_START_HOUR = 1;
const int UPLOAD_END_HOUR   = 4;


// === to check if any images left to upload on SD card ===
bool imagesLeftToUpload = true;


// === time variables ===
unsigned long initPIR_startTime             = 0;        // time at start of PIR initialsation
unsigned long lastAction_endTime            = 0;        // time when last action ended
unsigned long lastRing_endTime              = 0;
unsigned long surveillance_startTime        = 0;        // time at start of surveillance

const unsigned long surveillancePeriod      = 20000;    // allowed suveillance duration
const unsigned long initPIR_period          = 30000;    // duration of initialising PIR sensor
const unsigned long allowedStandbyDuration  = 60000;    // maximum allowed standby duration
const unsigned long timeSince_lastRing      = 2000;