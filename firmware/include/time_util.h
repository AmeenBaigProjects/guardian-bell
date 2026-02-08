#pragma once
#include <Arduino.h>

void initTime();

String getCurrentDateTime();

void scheduleRandomTimerWake();

bool timeToUpload();