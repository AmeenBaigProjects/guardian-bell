#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include "PubSubClient.h"

extern WiFiClient wifiClient;

extern PubSubClient mqtt;

void ensureMQTT();