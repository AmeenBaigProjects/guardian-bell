// === standard headers ===
#include <HTTPClient.h>


// === project headers ===
// --- corresponding header ---
#include "cloudinary.h"

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
WiFiClientSecure cloudinaryClient;


// === upload a JPEG file to cloudinary ===
bool uploadImageToCloudinary(File &file, String filename) {
    // --- skip certificate validation ---
    cloudinaryClient.setInsecure();

    // --- URL endpoint ---
    String url = "/v1_1/" + String(CLOUDINARY_CLOUD_NAME) + "/image/upload";

    // --- multipart boundary ---
    String boundary = "----ESP32CloudinaryBoundary";

    // --- build multipart body ---
    String head =
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"upload_preset\"\r\n\r\n" +
        String(CLOUDINARY_UPLOAD_PRESET) + "\r\n" +

        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"public_id\"\r\n\r\n" +
        filename + "\r\n" +

        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n";

    // --- build multipart tail ---
    String tail = "\r\n--" + boundary + "--\r\n";

    // --- determine total size of content ---
    uint32_t totalLength = head.length() + file.size() + tail.length();

    // --- connect to telegram ---
    DBG_PRINTLN("Connecting to " + String(cloudinaryHost));
    if (!cloudinaryClient.connect(cloudinaryHost, 443)) {
        DBG_PRINTLN("ERROR: Cloudinary connection failed");
        file.close();
        return false;
    }

    // --- send HTTP POST headers ---
    cloudinaryClient.print(
        "POST " + url + " HTTP/1.1\r\n"
        "Host: " + String(cloudinaryHost) + "\r\n"
        "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
        "Content-Length: " + String(totalLength) + "\r\n"
        "Connection: close\r\n\r\n"
    );

    // --- send multipart head ---
    cloudinaryClient.print(head);

    // --- send file binary ---
    uint8_t buf[1024];
    DBG_PRINTLN("Uploading JPEG to cloudinary...");
    while (file.available()) {
        int n = file.read(buf, sizeof(buf));
        cloudinaryClient.write(buf, n);
    }

    // --- send multipart tail ---
    cloudinaryClient.print(tail);

    // -- close file ---
    file.close();

    // --- read cloudinary response ---
    DBG_PRINTLN("Cloudinary response:");
    while (cloudinaryClient.connected()) {
        String line = cloudinaryClient.readStringUntil('\n');
        if (line == "\r") break;
    }
    String body = cloudinaryClient.readString();
    DBG_PRINTLN(body);

    DBG_PRINTLN("JPEG uploaded");
    return true;
}
