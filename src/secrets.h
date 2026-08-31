// =====================================================================
// PENTING: File ini berisi kredensial sensitif.
// JANGAN commit file ini ke Git! (sudah ada di .gitignore)
// =====================================================================
// Cara setup:
// 1. Copy file ini: cp secrets.example.h secrets.h
// 2. Isi semua nilai di bawah ini
// 3. Build & Upload
// =====================================================================

#ifndef SECRETS_H
#define SECRETS_H

// ——— WiFi ——————————————————————————————————————————
#define WIFI_SSID  "NAMA_WIFI_KAMU"
#define WIFI_PASS  "PASSWORD_WIFI_KAMU"

// ——— Adafruit IO (MQTT Broker) ————————————————————
// Daftar gratis di https://io.adafruit.com
// MQTT_USER  = username Adafruit IO kamu
// MQTT_PASS  = AIO Key (ada di dashboard Adafruit IO > My Key)
#define MQTT_SERVER  "io.adafruit.com"
#define MQTT_PORT     8883              // TLS/SSL port
#define MQTT_USER    "USERNAME_ADAFRUIT"
#define MQTT_PASS    "AIO_KEY_KAMU"

// ——— Topik MQTT ————————————————————————————————————
// Format Adafruit IO: {username}/feeds/{feed-name}
// Sesuaikan username dan nama feed dengan yang kamu buat di dashboard
#define TOPIC_TELEMETRY  "USERNAME_ADAFRUIT/feeds/wq-telemetry"
#define TOPIC_ALERT      "USERNAME_ADAFRUIT/feeds/wq-alert"
#define TOPIC_STATUS     "USERNAME_ADAFRUIT/feeds/wq-status"

// ——— OTA Firmware Update ———————————————————————————
// URL ke file .bin firmware terbaru
// Jika tidak punya server OTA, set ke URL yang tidak ada agar tidak error fatal
// (checkForUpdates akan menerima HTTP 404 dan skip dengan aman)
#define OTA_FIRMWARE_URL  "https://example.com/firmware/wqm_latest.bin"

#endif // SECRETS_H
