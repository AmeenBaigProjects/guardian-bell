// === project headers ===
// --- corresponding header ---
#include "settings.h"


/// === firmware version ===
const String FW_VERSION = "v1.0.0-beta.3.2";


/// === acceptable number of motion detections in one boot ===
const int acceptableDetections = 20;


// === time settings ===
/// --- ntp server url ---
const char* ntpServer = "uk.pool.ntp.org";

/// --- offset from gmt in hours ---
const long  gmtOffset_sec = 0;

/// --- offset for daylight saving in seconds ---
const int  daylightOffset_sec = 3600;


/// === filename of image captured at latest doorbell ring ===
const String lastRingCaptureFilename = "latest_ring_capture";


// === telegram ===
/// --- telegram host url ---
const char* telegramHost = "api.telegram.org";

/// --- caption sent to telegram with latest ring capture ---
String captionText = "🔔 Someone's at the door!";


/// === cloudinary host url ===
const char* cloudinaryHost = "api.cloudinary.com";


// === time window to commence upload ===
/// --- start hour of upload window ---
const int UPLOAD_START_HOUR = 1;
/// --- end hour of upload window ---
const int UPLOAD_END_HOUR   = 4;


/// === to check if any images left to upload on SD card ===
bool imagesLeftToUpload = true;


// === time variables ===
/// --- allowed suveillance duration ---
const unsigned long surveillancePeriod      = 15000;

/// --- time allowed to warm up PIR sensor ---
const unsigned long warmUpPeriod            = 20000;

/// --- allowed standby duration ---
const unsigned long allowedStandbyDuration  = 60000;

/// --- minimum time to pass since last ring ---
const unsigned long timeSinceLastRing       = 2000;