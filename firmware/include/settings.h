#pragma once
#include <Arduino.h>

extern const String FW_VERSION;

extern const int acceptableDetections;

extern const char* ntpServer;

extern const long  gmtOffset_sec;

extern const int   daylightOffset_sec;

extern const String lastRingCaptureFilename;

extern const char* telegramHost;

extern String captionText;

extern const char* cloudinaryHost;

extern const int UPLOAD_START_HOUR;

extern const int UPLOAD_END_HOUR;

extern bool imagesLeftToUpload;

extern unsigned long lastActionTime;

extern unsigned long lastRingTime;

extern const unsigned long surveillancePeriod;

extern const unsigned long warmUpPeriod;

extern const unsigned long allowedStandbyDuration;

extern const unsigned long timeSinceLastRing; 