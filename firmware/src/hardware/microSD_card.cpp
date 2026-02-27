// === standard headers ===
// --- SD card access via SD_MMC interface ---
#include <SD_MMC.h>


// === project headers ===
// --- corresponding header ---
#include "microSD_card.h"

// --- utilities ---
#include "debug.h"
#include "error.h"


// === initialise micro SD card ===
void initMicroSD() {
    DBG_PRINTLN("Initialising SD card...");

    // --- begin connection to micro SD card in 1-bit mode ---
    if (!SD_MMC.begin("/sdcard", true)) {
        error("Failed to initialise SD card", true);
    }
    else {
        DBG_PRINTLN("SD card initialised");
    }

    // --- check if SD card is inserted & readable ---
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        error("SD Card corrupted or not detected", true);
    }
}
