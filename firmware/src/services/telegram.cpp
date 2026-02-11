// === standard headers ===
#include <SD_MMC.h>
#include <HTTPClient.h>


// === project headers ===
// --- corresponding header ---
#include "telegram.h"

// --- secrets_example.h for reference ---
#include "secrets.h"

// --- configuration ---
#include "settings.h"

// --- network ---
#include "wifi.h"

// --- utilities ---
#include "debug.h"
#include "error.h"


// === WIFI client setup ===
WiFiClientSecure telegramClient;


// === send error message to telegram ===
void sendMsgToTelegram(const String& msg) {
    // --- ensure active WiFi connection ---
    initWifi();

    // --- skip certificate validation ---
    telegramClient.setInsecure();

    // --- URL endpoint ---
    String url =
        "/bot" + String(TELEGRAM_BOT_TOKEN) +
        "/sendMessage?chat_id=" + String(TELEGRAM_CHAT_ID) +
        "&text=" "ERROR: " + msg;

    // --- connect to telegram ---
    if (!telegramClient.connect(telegramHost, 443)) {
        DBG_PRINTLN("ERROR: Telegram error notify failed");
        return;
    }

    // --- simple GET ---
    telegramClient.print(
        "GET " + url + " HTTP/1.1\r\n"
        "Host: " + String(telegramHost) + "\r\n"
        "Connection: close\r\n\r\n"
    );

    // --- stop client ---
    telegramClient.stop();
}


void sendImageToTelegram() {
    // --- skip certificate validation ---
    telegramClient.setInsecure();

    // --- open latest ring capture JPEG ---
    File file = SD_MMC.open("/IMG_" + latestRingCapture_filename + ".jpg");
    if (!file) {
        error("Failed to open JPEG file", false);
    }
    else {
        DBG_PRINTLN("Opened JPEG: " + String(file.name()));
    }

    // --- URL endpoint ---
    String url = "/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendPhoto";

    // --- multipart boundary ---
    String boundary = "----ESP32CAMBoundary";

    // --- build multipart body ---
    String head =
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
        String(TELEGRAM_CHAT_ID) + "\r\n" +
        
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" +
        captionText + "\r\n" +
        
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"photo\"; filename=\"doorbell.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n";

    // --- build multipart tail ---
    String tail = "\r\n--" + boundary + "--\r\n";

    // --- determine total size of content ---
    uint32_t totalLength = head.length() + file.size() + tail.length();

    // --- connect to telegram ---
    const char* telegramHost = "api.telegram.org";
    DBG_PRINTLN("Connecting to " + String(telegramHost));
    if (!telegramClient.connect(telegramHost, 443)) {
        DBG_PRINTLN("ERROR: Telegram connection failed");
        file.close();
        return;
    }

    // --- send HTTP POST headers ---
    telegramClient.print(
        "POST " + url + " HTTP/1.1\r\n"
        "Host: " + String(telegramHost) + "\r\n"
        "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
        "Content-Length: " + String(totalLength) + "\r\n"
        "Connection: close\r\n\r\n"
    );

    // --- send multipart head ---
    telegramClient.print(head);

    // --- send file binary ---
    uint8_t buf[1024];
    DBG_PRINTLN("Sending JPEG to telegram...");
    while (file.available()) {
        int n = file.read(buf, sizeof(buf));
        telegramClient.write(buf, n);
    }

    // --- send multipart tail ---
    telegramClient.print(tail);

    // -- close file ---
    file.close();

    // --- read telegram response ---
    DBG_PRINTLN("Telegram response:");
    while (telegramClient.connected()) {
        String line = telegramClient.readStringUntil('\n');
        if (line == "\r") break;
    }
    String body = telegramClient.readString();
    DBG_PRINTLN(body);

    // --- stop client ---
    telegramClient.stop();

    DBG_PRINTLN("JPEG sent");
}