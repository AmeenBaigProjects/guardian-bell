// === project headers ===
// --- corresponding header ---
#include "button_interrupt.h"


/// === to check doorbell interrupt ===
volatile bool doorbellInterrupted = false; 


/// === push button interrupt handler ===
void IRAM_ATTR handleButtonInterrupt() { 
    doorbellInterrupted = true;
}