#include "storage.h"
#include "connectivity.h"
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <ArduinoJson.h>

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #error "File secrets.h tidak ditemukan!"
#endif

static RTC_DS3231 rtc;
static bool sdAvailable  = false;
static bool rtcAvailable = false;

// ——— Inisialisasi ————————————————————————————————
void initStorage() {
    // Inisialisasi RTC
    if (!rtc.begin()) {
        Serial.println("PERINGATAN: RTC tidak ditemukan! Timestamp akan bernilai 0.");
        rtcAvailable = false;
    } else {
        rtcAvailable = true;
        if (rtc.lostPower()) {
            // RTC kehilangan daya (baterai habis) — set ke waktu kompilasi sebagai fallback
            Serial.println("RTC kehilangan daya. Mengatur ke waktu kompilasi.");
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        Serial.printf("RTC OK. Waktu sekarang: %s\n", 
                      rtc.now().timestamp(DateTime::TIMESTAMP_FULL).c_str());
    }

    // Inisialisasi SD Card
    if (!SD.begin(PIN_SD_CS)) {
        Serial.println("PERINGATAN: SD Card tidak ditemukan! Logging dinonaktifkan.");
        sdAvailable = false;
    } else {
        Serial.println("SD Card OK.");
        sdAvailable = true;

        // Buat header file jika belum ada
        if (!SD.exists("/log.csv")) {
            File f = SD.open("/log.csv", FILE_WRITE);
            if (f) {
                f.println("timestamp,tds_ppm,turbidity_ntu,ph,battery_v,alert_flag,sent_to_cloud");
                f.close();
                Serial.println("File log.csv baru dibuat.");
            }
        }
    }
}

// ——— Logging ke SD Card ——————————————————————————
void logDataToSD(SensorData data, float batteryVoltage, bool isAlert, bool sentToCloud) {
    if (!sdAvailable) return;

    File file = SD.open("/log.csv", FILE_APPEND);
    if (!file) {
        Serial.println("ERROR: Gagal membuka log.csv untuk menulis.");
        return;
    }

    // Format timestamp ISO 8601
    char timestampStr[25];
    if (rtcAvailable) {
        DateTime now = rtc.now();
        snprintf(timestampStr, sizeof(timestampStr), "%04d-%02d-%02dT%02d:%02d:%02d",
                 now.year(), now.month(), now.day(),
                 now.hour(), now.minute(), now.second());
    } else {
        snprintf(timestampStr, sizeof(timestampStr), "1970-01-01T00:00:00");
    }

    // Format: timestamp,tds,turbidity,ph,battery_v,alert,sent
    file.printf("%s,%.1f,%.1f,%.2f,%.2f,%d,%d\n",
                timestampStr,
                data.tds,
                data.turbidity,
                data.ph,
                batteryVoltage,
                isAlert ? 1 : 0,
                sentToCloud ? 1 : 0);

    file.close();
    Serial.println("Data tersimpan ke SD Card.");
}

// ——— Timestamp Unix ——————————————————————————————
uint32_t getCurrentTimestamp() {
    if (rtcAvailable) {
        return rtc.now().unixtime();
    }
    return 0;
}

// ——— Sinkronisasi Data Offline ———————————————————
// Strategi: baca log.csv baris per baris, kirim ulang semua baris dengan sent_to_cloud == 0.
// Jika berhasil, tulis ulang file tanpa baris tersebut (replace file).
// Pendekatan ini aman di RAM karena kita hanya memproses 1 baris sekaligus.
void syncOfflineData() {
    if (!sdAvailable) return;

    PubSubClient& mqtt = getMqttClient();
    if (!mqtt.connected()) return; // hanya jika MQTT sudah siap

    // Buka file untuk baca
    File srcFile = SD.open("/log.csv", FILE_READ);
    if (!srcFile) {
        Serial.println("syncOfflineData: tidak bisa membuka log.csv.");
        return;
    }

    // File sementara untuk baris yang sudah terkirim (header + baris sent=1)
    SD.remove("/log_tmp.csv");
    File tmpFile = SD.open("/log_tmp.csv", FILE_WRITE);
    if (!tmpFile) {
        Serial.println("syncOfflineData: tidak bisa membuat log_tmp.csv.");
        srcFile.close();
        return;
    }

    int totalUnsent  = 0;
    int totalSynced  = 0;
    bool isFirstLine = true;

    while (srcFile.available()) {
        String line = srcFile.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) continue;

        // Selalu salin header
        if (isFirstLine) {
            tmpFile.println(line);
            isFirstLine = false;
            continue;
        }

        // Cek apakah baris ini sudah terkirim (karakter terakhir == '1')
        bool alreadySent = line.endsWith(",1");

        if (alreadySent) {
            // Pertahankan baris yang sudah terkirim
            tmpFile.println(line);
            continue;
        }

        // Baris belum terkirim — coba parse dan kirim
        totalUnsent++;

        // Parse CSV: timestamp,tds,turbidity,ph,battery_v,alert,sent
        // Gunakan strtok pada copy char array (strtok bersifat destruktif)
        char buf[128];
        line.toCharArray(buf, sizeof(buf));

        char* tok_ts   = strtok(buf,  ",");
        char* tok_tds  = strtok(NULL, ",");
        char* tok_turb = strtok(NULL, ",");
        char* tok_ph   = strtok(NULL, ",");
        char* tok_batt = strtok(NULL, ",");
        char* tok_alrt = strtok(NULL, ",");
        // tok terakhir (sent) kita abaikan

        if (!tok_ts || !tok_tds || !tok_turb || !tok_ph || !tok_batt || !tok_alrt) {
            // Baris korup / tidak lengkap — pertahankan apa adanya
            tmpFile.println(line);
            continue;
        }

        // Bangun JSON payload ulang
        StaticJsonDocument<256> doc;
        doc["ts"]     = tok_ts;
        doc["tds"]    = atof(tok_tds);
        doc["turb"]   = atof(tok_turb);
        doc["ph"]     = atof(tok_ph);
        doc["batt_v"] = atof(tok_batt);
        doc["alert"]  = atoi(tok_alrt);
        doc["resync"] = true; // penanda bahwa ini data yang dikirim ulang

        char payload[256];
        serializeJson(doc, payload);

        bool published = mqtt.publish(TOPIC_TELEMETRY, payload, false);

        if (published) {
            totalSynced++;
            // Tandai baris ini sebagai sudah terkirim (ganti ,0 di akhir dengan ,1)
            String updatedLine = line.substring(0, line.lastIndexOf(',') + 1) + "1";
            tmpFile.println(updatedLine);
            Serial.printf("syncOfflineData: Berhasil kirim baris ke-%d\n", totalSynced);
        } else {
            // Gagal kirim — pertahankan baris dengan sent=0 untuk percobaan berikutnya
            tmpFile.println(line);
            Serial.println("syncOfflineData: Gagal kirim, akan dicoba lagi nanti.");
        }

        // Beri waktu MQTT broker memproses (yield mencegah WDT reset pada loop panjang)
        mqtt.loop();
        yield();
    }

    srcFile.close();
    tmpFile.close();

    // Ganti file asli dengan file yang sudah diupdate
    SD.remove("/log.csv");
    // SD library Arduino tidak punya rename, pakai pendekatan copy kembali
    File newSrc  = SD.open("/log_tmp.csv", FILE_READ);
    File newDest = SD.open("/log.csv",     FILE_WRITE);
    if (newSrc && newDest) {
        while (newSrc.available()) {
            newDest.write(newSrc.read());
        }
    }
    if (newSrc)  newSrc.close();
    if (newDest) newDest.close();
    SD.remove("/log_tmp.csv");

    Serial.printf("syncOfflineData selesai: %d baris offline, %d berhasil disinkronisasi.\n",
                  totalUnsent, totalSynced);
}
