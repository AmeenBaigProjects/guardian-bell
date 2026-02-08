#pragma once

// === debug macros ===
// --- 1 or 0 to turn serial debugging ON or OFF respectively ---
#define SERIAL_DEBUG 1

#if SERIAL_DEBUG
  #define DBG_SERIAL_BEGIN(x)   Serial.begin(x)
  #define DBG_PRINT(x)          Serial.print(x)
  #define DBG_PRINTLN(x)        Serial.println(x)
  #define DBG_DELAY(X)          delay(X)
#else
  #define DBG_BEGIN_SERIAL(x)
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
  #define DBG_DELAY(X)
#endif