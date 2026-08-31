#include <Arduino.h>
#include <esp_task_wdt.h>
#include "sensors.h"
#include "alerts.h"
#include "storage.h"
#include "connectivity.h"
#include "power_mgmt.h"

// ——— Konfigurasi ———————————————————————————————
// Durasi deep sleep antar siklus pembacaan
#define SLEEP_TIME_SECONDS  600ULL  // 10 menit

// Watchdog timeout: harus lebih panjang dari total waktu satu siklus
// Estimasi: WiFi (15s) + MQTT (5s) + OTA check (10s) + overhead = ~60s cukup
// Naikkan ke 90s untuk safety margin
#define WDT_TIMEOUT_SECONDS  90

void setup() {
    Serial.begin(115200);
    delay(100); // beri waktu Serial monitor terhubung
    Serial.println("\n========================================");
    Serial.println("  Water Quality Monitor  v1.0");
    Serial.println("========================================\n");

    // ——— Watchdog Timer ——————————————————————————
    // Jika setup() atau loop() tidak selesai dalam WDT_TIMEOUT_SECONDS,
    // ESP32 akan otomatis reset — mencegah firmware hang selamanya
    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
    esp_task_wdt_add(NULL);
    Serial.printf("Watchdog aktif (%d detik timeout).\n", WDT_TIMEOUT_SECONDS);

    // ——— Inisialisasi Modul (urutan penting!) —————
    initPowerMgmt();   // MOSFET OFF, ADC attenuation
    initSensors();     // pinMode ADC sensor
    initAlerts();      // LED, Buzzer, LCD
    initStorage();     // SD Card + RTC
    initConnectivity();// MQTT server config
}

void loop() {
    // Feed watchdog di awal setiap siklus
    esp_task_wdt_reset();

    Serial.println("\n--- Siklus Pembacaan Baru ---");

    // ——— Step 1: Baca Sensor ——————————————————————
    powerOnSensors();              // Hidupkan sensor via MOSFET
    esp_task_wdt_reset();          // Feed WDT (powerOn + delay 500ms sudah makan waktu)

    SensorData data      = readSensors();
    float batteryVoltage = readBatteryVoltage();

    powerOffSensors();             // Matikan sensor segera untuk hemat daya

    // ——— Step 2: Evaluasi & Tampilkan Lokal ————————
    bool isAlert = checkThresholds(data);
    setAlertState(isAlert);
    updateDisplay(data, isAlert);

    Serial.printf("Status: TDS=%.1f ppm | Turb=%.1f NTU | pH=%.2f | Batt=%.2fV | Alert=%s\n",
                  data.tds, data.turbidity, data.ph, batteryVoltage,
                  isAlert ? "YA" : "TIDAK");

    // ——— Step 3: Kirim ke Cloud ———————————————————
    esp_task_wdt_reset(); // Feed WDT sebelum operasi jaringan yang bisa lambat

    uint32_t ts         = getCurrentTimestamp();
    bool sentToCloud    = publishTelemetry(data, batteryVoltage, ts);

    if (isAlert && sentToCloud) {
        publishAlert(data, "threshold_violation", ts);
    }

    // ——— Step 4: Sync & OTA (hanya jika cloud terhubung) ——
    if (sentToCloud) {
        esp_task_wdt_reset();
        syncOfflineData();

        esp_task_wdt_reset();
        checkForUpdates(); // Jika ada update, ESP.restart() dipanggil di sini (tidak balik)
    }

    // ——— Step 5: Log ke SD Card ———————————————————
    logDataToSD(data, batteryVoltage, isAlert, sentToCloud);

    // ——— Step 6: Bersihkan & Tidur ————————————————
    silenceAlerts();           // Matikan LED, Buzzer, LCD backlight sebelum sleep
    disconnectConnectivity();  // Putuskan WiFi radio (hemat daya)

    Serial.printf("Siklus selesai. Tidur %llu detik...\n\n", SLEEP_TIME_SECONDS);

    // goToDeepSleep tidak pernah kembali — setup() akan dipanggil ulang saat bangun
    goToDeepSleep(SLEEP_TIME_SECONDS);
}
