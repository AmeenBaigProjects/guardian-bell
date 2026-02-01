#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "PubSubClient.h"
#include <time.h>
#include <HTTPClient.h>

// === ESP32-CAM pin configuration ===
// source pin to wake ESP32-CAM from deep-sleep
const int WAKE_PIN = 4;

// I2C pins
const int SCL_PIN = 12;          // I2C SCL to MCP23X17
const int SDA_PIN = 13;          // I2C SDA to MCP23X17

// camera pins
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22


// === MCP23017 configuration ===
Adafruit_MCP23X17 mcp;

// wake & input sources
const int BTN_PIN = 0;          // push button input
const int PIR_PIN = 1;          // PIR input

// peripherals
const int RED_LED_PIN   = 3;    // external red LED
const int BLUE_LED_PIN  = 4;    // external blue LED
const int BUZZER_PIN  = 5;      // piezo buzzer


// === time configuration ===
const char* ntpServer = "uk.pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;


// === filename of image captured at latest doorbell ring ===
const String latestRingCapture_filename = "latest_ring_capture";


// === variable to check if images were uploaded to google drive ===
bool uploadCheck = false;


// === telegram ===
// --- host url ---
const char* telegramHost = "api.telegram.org"; 
// --- caption sent to telegram with latest ring capture ---
String captionText = "🔔 Someone's at the door!";


// === cloudinary ===
// --- host url ---
const char* cloudinaryHost = "api.cloudinary.com";


// === time variables ===
unsigned long boot_startTime            = 0;        // time at start of boot
unsigned long initPIR_startTime         = 0;        // time at start of PIR initialsation
unsigned long lastAction_endTime        = 0;        // time when last action ended
unsigned long lastRing_endTime          = 0;
unsigned long surveillance_startTime    = 0;        // time at start of surveillance

const unsigned long surveillancePeriod  = 20000;    // allowed suveillance duration
const unsigned long initPIR_period      = 30000;    // duration of initialising PIR sensor
const unsigned long standbyPeriod       = 60000;    // allowed standby duration
const unsigned long gapSince_lastRing   = 2000;


// === WIFI client setup ===
WiFiClient wifiClient;
WiFiClientSecure telegramClient;
WiFiClientSecure cloudinaryClient;


// === MQTT Setup ===
PubSubClient mqtt(wifiClient);