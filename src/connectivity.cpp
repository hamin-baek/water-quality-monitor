#include "connectivity.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #error "File secrets.h tidak ditemukan! Salin src/secrets.example.h ke src/secrets.h dan isi kredensial kamu."
#endif

// ——— Objek koneksi ———————————————————————————————
static WiFiClientSecure secureClient;
static PubSubClient     mqttClient(secureClient);

// Accessor publik agar modul lain (storage) bisa publish tanpa duplikasi instance
PubSubClient& getMqttClient() {
    return mqttClient;
}

// ——— WiFi ————————————————————————————————————————
void initConnectivity() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setBufferSize(512); // buffer lebih besar untuk payload JSON
    Serial.println("Connectivity initialized.");
}

bool connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) return true;

    Serial.printf("Menghubungkan ke WiFi: %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Timeout 15 detik (30 x 500ms)
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi Terhubung! IP: %s\n", WiFi.localIP().toString().c_str());
        // CATATAN KEAMANAN: setInsecure() menonaktifkan validasi sertifikat TLS.
        // Untuk production deployment, ganti dengan setCACert(ca_cert) menggunakan
        // sertifikat root CA dari broker MQTT kamu (contoh: Adafruit IO root CA).
        secureClient.setInsecure();
        return true;
    }

    Serial.println("Gagal terhubung ke WiFi.");
    return false;
}

// ——— MQTT ————————————————————————————————————————
bool connectMQTT() {
    if (mqttClient.connected()) return true;
    if (WiFi.status() != WL_CONNECTED) return false;

    // Client ID unik: prefix + 4 hex random agar tidak konflik di broker
    char clientId[24];
    snprintf(clientId, sizeof(clientId), "WQM-%08X", (uint32_t)esp_random());

    Serial.printf("Menghubungkan ke MQTT broker %s...", MQTT_SERVER);

    // LWT (Last Will & Testament): broker otomatis publish "offline" jika koneksi putus tiba-tiba
    if (mqttClient.connect(clientId, MQTT_USER, MQTT_PASS, TOPIC_STATUS, 1, true, "offline")) {
        Serial.println(" Terhubung!");
        mqttClient.publish(TOPIC_STATUS, "online", true);
        return true;
    }

    Serial.printf(" Gagal (rc=%d)\n", mqttClient.state());
    return false;
}

void disconnectConnectivity() {
    if (mqttClient.connected()) {
        mqttClient.publish(TOPIC_STATUS, "offline", true);
        mqttClient.disconnect();
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF); // Matikan WiFi radio untuk hemat daya sebelum sleep
}

// ——— Publish Data ————————————————————————————————
bool publishTelemetry(SensorData data, float batteryVoltage, uint32_t timestamp) {
    if (!connectWiFi() || !connectMQTT()) return false;

    StaticJsonDocument<256> doc;
    doc["ts"]     = timestamp;
    doc["tds"]    = serialized(String(data.tds, 1));
    doc["turb"]   = serialized(String(data.turbidity, 1));
    doc["ph"]     = serialized(String(data.ph, 2));
    doc["batt_v"] = serialized(String(batteryVoltage, 2));
    doc["alert"]  = data.isError;

    char buffer[256];
    size_t len = serializeJson(doc, buffer);

    Serial.printf("Mengirim telemetri (%d bytes): %s\n", len, buffer);
    return mqttClient.publish(TOPIC_TELEMETRY, buffer, false);
}

bool publishAlert(SensorData data, const char* reason, uint32_t timestamp) {
    if (!connectWiFi() || !connectMQTT()) return false;

    StaticJsonDocument<256> doc;
    doc["ts"]     = timestamp;
    doc["reason"] = reason;
    doc["tds"]    = serialized(String(data.tds, 1));
    doc["turb"]   = serialized(String(data.turbidity, 1));
    doc["ph"]     = serialized(String(data.ph, 2));

    char buffer[256];
    size_t len = serializeJson(doc, buffer);

    Serial.printf("Mengirim alert (%d bytes): %s\n", len, buffer);
    return mqttClient.publish(TOPIC_ALERT, buffer, false);
}

// ——— OTA Update ——————————————————————————————————
void checkForUpdates() {
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.printf("Mengecek OTA update di: %s\n", OTA_FIRMWARE_URL);

    HTTPClient http;
    secureClient.setInsecure();
    http.setTimeout(10000); // 10 detik timeout

    if (!http.begin(secureClient, OTA_FIRMWARE_URL)) {
        Serial.println("OTA: Gagal membuka koneksi HTTP.");
        return;
    }

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        if (contentLength <= 0) {
            Serial.println("OTA: Content-Length tidak valid, batal.");
            http.end();
            return;
        }

        Serial.printf("OTA: Memulai update, ukuran firmware: %d bytes\n", contentLength);

        if (!Update.begin(contentLength)) {
            Serial.printf("OTA: Tidak cukup ruang (error: %d)\n", Update.getError());
            http.end();
            return;
        }

        // Stream binary langsung dari HTTP ke flash
        size_t written = Update.writeStream(http.getStream());

        if (written != (size_t)contentLength) {
            Serial.printf("OTA: Download tidak lengkap (%d/%d bytes)\n", written, contentLength);
            Update.abort();
            http.end();
            return;
        }

        if (Update.end() && Update.isFinished()) {
            Serial.println("OTA: Update berhasil! Merestart sistem...");
            http.end();
            ESP.restart(); // Tidak akan kembali ke sini
        } else {
            Serial.printf("OTA: Gagal menyelesaikan update (error: %d)\n", Update.getError());
        }
    } else if (httpCode == HTTP_CODE_NOT_FOUND) {
        Serial.println("OTA: Tidak ada firmware baru.");
    } else {
        Serial.printf("OTA: HTTP error %d\n", httpCode);
    }

    http.end();
}
