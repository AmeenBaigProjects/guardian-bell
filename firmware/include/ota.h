#pragma once
#include <Arduino.h>

void checkForFirmwareUpdate();

bool performFirmwareUpdateOTA(String rmtVersion);