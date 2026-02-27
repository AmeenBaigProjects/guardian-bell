#pragma once
#include <Arduino.h>

// === ESP32-CAM pin configuration ===
// flash LED onboard the ESP32-CAM
const int FLASH_LED_PIN = 4;

// source pin to wake ESP32-CAM from deep-sleep
const int WAKE_PIN = 4;

// --- I2C SCL to MCP23X17 ---
const int SCL_PIN = 12;
// --- I2C SDA to MCP23X17 ---         
const int SDA_PIN = 13;

// --- camera pins ---
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
// --- input sources ---
const int BTN_PIN = 0;          // push button input
const int PIR_PIN = 1;          // PIR input

// --- peripherals ---
const int RED_LED_PIN   = 3;    // external red LED
const int BLUE_LED_PIN  = 4;    // external blue LED
const int BUZZER_PIN  = 5;      // piezo buzzer