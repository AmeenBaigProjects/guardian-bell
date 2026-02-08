#pragma once
#include <Arduino.h>
#include <SD_MMC.h>

bool uploadImageToCloudinary(File &file, String filename);