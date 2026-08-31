# Water Quality Monitor

Perangkat IoT untuk memantau kualitas air di desa-desa yang butuh data real-time. Dirancang buat lokasi terpencil tanpa akses listrik PLN.

---

## Daftar Isi

- [Tentang Proyek](#tentang-proyek)
- [Fitur](#fitur)
- [Hardware](#hardware)
- [Arsitektur Sistem](#arsitektur-sistem)
- [Mulai Menggunakan](#mulai-menggunakan)
- [Konfigurasi](#konfigurasi)
- [Struktur Direktori](#struktur-direktori)
- [Data Schema](#data-schema)
- [Power Budget](#power-budget)
- [Utang Teknis](#utang-teknis)
- [Lisensi](#lisensi)

---

## Tentang Proyek

Proyek ini bagian dari inisiatif **IoT Desa**, sistem pemantauan terpadu untuk desa-desa di Indonesia. Water Quality Monitor adalah **Proyek 1** dari tiga:

| # | Proyek | Status |
|---|--------|--------|
| 1 | Water Quality Monitor (proyek ini) | ✅ Build sukses |
| 2 | Power Outage Predictor | ✅ Build sukses |
| 3 | Forest Fire Warning System | 🔲 Belum dimulai |

Device membaca sensor TDS, turbidity, dan pH secara berkala. Data disimpan ke SD card, ditampilkan di LCD, dan dikirim ke cloud via MQTT over TLS. Kalau threshold dilanggar, buzzer dan LED langsung nyala tanpa nunggu respons cloud (fail-safe).

Semua jalan dari solar panel + baterai Li-ion dengan konsumsi sekitar 200 mAh/hari.

---

## Fitur

- Telemetri cloud via MQTT over TLS (port 8883) setiap 10 menit
- Alert lokal (buzzer + LED) saat threshold dilanggar, tidak menunggu cloud
- Log SD card (CSV) sebagai backup saat WiFi tidak tersedia
- Deep sleep antar cycle untuk efisiensi daya
- OTA update via HTTPS, perbaiki firmware tanpa perlu datang ke lokasi fisik
- LCD 16x2 untuk pembacaan langsung oleh warga
- RTC DS3231 untuk timestamp akurat meski device restart atau sleep
- Store-and-forward: data gagal kirim disimpan, dikirim ulang di cycle berikutnya
- Watchdog timer: auto-restart jika firmware hang

---

## Hardware

### Bill of Materials (BOM)

| Komponen | Fungsi | Est. Harga |
|---|---|---|
| ESP32 DevKit V1 | Mikrokontroler + WiFi | Rp50.000 |
| Sensor TDS + modul | Total Dissolved Solids (ppm) | Rp75.000–120.000 |
| Sensor Turbidity | Kekeruhan air (NTU) | Rp60.000–90.000 |
| Sensor pH + probe | Keasaman air | Rp90.000–150.000 |
| LCD 16x2 I2C | Display lokal | Rp30.000 |
| Buzzer aktif | Alert suara | Rp5.000 |
| LED merah/kuning/hijau | Indikator visual | Rp5.000 |
| Modul Micro SD + kartu 8GB | Backup log lokal | Rp30.000 |
| RTC DS3231 | Timestamp akurat | Rp20.000 |
| Solar panel 5-10W | Sumber energi utama | Rp60.000–100.000 |
| Solar charge controller | Kontrol pengisian baterai | Rp30.000–70.000 |
| 2x Baterai 18650 Li-ion | Penyimpan energi | Rp40.000–90.000 |
| Buck/boost converter | Stabilkan tegangan | Rp15.000–30.000 |
| MOSFET IRLZ44N | Power-switching sensor rail | Rp10.000–20.000 |
| Box IP65 + aksesoris | Enclosure tahan air | Rp60.000–90.000 |

**Total estimasi: Rp610.000–950.000 per unit**

### Board & Partition

- **Board:** `esp32doit-devkit-v1`
- **Partisi:** `min_spiffs.csv`, skema OTA dual-slot (app0/app1), total flash 1.92MB
- **Flash usage:** ~50.3% (cukup ruang untuk OTA update)

---

## Arsitektur Sistem

```
+--------------------------+   WiFi (MQTT/TLS)    +----------------------------+
|      DEVICE LAYER        | ------------------>  |    CLOUD/BACKEND LAYER     |
|                          |                      |                            |
|  ESP32 + sensor TDS/     |                      |  MQTT Broker (TLS :8883)   |
|  Turbidity/pH + LCD +    | <------------------  |  -> Backend/Processor      |
|  Buzzer/LED + SD + RTC   | (command/OTA trigger)|  -> Database (time-series) |
|  + solar/baterai         |                      |  -> Dashboard web          |
+--------------------------+                      |  -> Alert (WA/Telegram)    |
                                                  +----------------------------+
```

### State Machine per Wake Cycle (10 menit)

```
[DEEP SLEEP] -> [BOOT/WAKE] -> [INIT] -> [SENSOR ON] -> [BACA SENSOR]
     ^                                                         |
     |                                                         v
     |                                                   [SENSOR OFF]
     |                                                         |
     |                                               [CEK THRESHOLD]
     |                                         (Buzzer/LED lokal, fail-safe)
     |                                                         |
     |                                               [LOG ke SD card]
     |                                                         |
     |                                        +----------------+---------------+
     |                                   WiFi OK                         WiFi Gagal
     |                                        |                               |
     |                                 [MQTT Publish]              [Simpan ke antrian]
     |                                        |                               |
     +----------------------------------------+-------------------------------+
                                         [DEEP SLEEP]
```

---

## Mulai Menggunakan

### Prasyarat

- [PlatformIO](https://platformio.org/) (extension VS Code atau CLI)
- Python 3.x (untuk PlatformIO)
- MQTT broker yang mendukung TLS (rekomendasi: [Adafruit IO](https://io.adafruit.com/) untuk pilot awal)

### Instalasi

```bash
# Clone repo
git clone https://github.com/<username>/water-quality-monitor.git
cd water-quality-monitor

# Salin file konfigurasi rahasia
cp src/secrets.example.h src/secrets.h
```

Edit `src/secrets.h` dengan kredensial kamu (lihat bagian [Konfigurasi](#konfigurasi)).

```bash
# Build
pio run

# Upload ke board (pastikan board terhubung via USB)
pio run --target upload

# Monitor serial output
pio device monitor
```

---

## Konfigurasi

Salin `src/secrets.example.h` ke `src/secrets.h` dan isi nilai berikut:

```cpp
// WiFi
#define WIFI_SSID     "nama_jaringan_wifi"
#define WIFI_PASS     "password_wifi"

// MQTT Broker (TLS, port 8883)
#define MQTT_SERVER   "io.adafruit.com"
#define MQTT_PORT     8883
#define MQTT_USER     "username_adafruit"
#define MQTT_PASS     "aio_key_adafruit"

// MQTT Topics
#define TOPIC_TELEMETRY  "desa/namedesa/waterquality/device01/telemetry"
#define TOPIC_ALERT      "desa/namedesa/waterquality/device01/alert"
#define TOPIC_STATUS     "desa/namedesa/waterquality/device01/status"

// OTA
#define OTA_FIRMWARE_URL  "https://your-server.com/firmware/wqm-latest.bin"
```

> **Perhatian:** Jangan commit `secrets.h` ke repository. File ini sudah ditambahkan ke `.gitignore`.

---

## Struktur Direktori

```
water-quality-monitor/
├── src/
│   ├── main.cpp              # Orkestrasi state machine utama
│   ├── sensors.cpp/.h        # Baca & kalibrasi TDS/Turbidity/pH
│   ├── connectivity.cpp/.h   # WiFi, MQTT, TLS, OTA
│   ├── storage.cpp/.h        # SD card log, antrian offline
│   ├── alerts.cpp/.h         # Evaluasi threshold, buzzer/LED/LCD
│   ├── power_mgmt.cpp/.h     # MOSFET sensor rail, deep sleep, baterai
│   ├── secrets.h             # kredensial sensitif (gitignore'd)
│   └── secrets.example.h     # template konfigurasi
├── platformio.ini            # konfigurasi build PlatformIO
├── spek-teknis-water-quality-monitor.md  # spesifikasi teknis lengkap
└── LICENSE
```

---

## Data Schema

### Payload Telemetri (MQTT topic: `.../telemetry`)

```json
{
  "ts":     1735459200,
  "tds":    245.3,
  "turb":   3.2,
  "ph":     7.1,
  "batt_v": 3.87,
  "alert":  false
}
```

### Payload Alert (MQTT topic: `.../alert`)

```json
{
  "ts":     1735459200,
  "reason": "tds_high",
  "tds":    620.5,
  "turb":   3.2,
  "ph":     7.1
}
```

### Log SD Card (CSV)

```
timestamp,tds_ppm,turbidity_ntu,ph,battery_v,alert_flag,sent_to_cloud
2026-08-29T14:10:00,245.3,3.2,7.1,3.87,0,1
```

Kolom `sent_to_cloud` dipakai firmware untuk menandai data yang masih perlu dikirim ulang.

---

## Power Budget

| Kondisi | Konsumsi |
|---|---|
| Deep sleep | ~0.15 mA |
| Aktif (WiFi + sensor) | ~180 mA rata-rata |
| Durasi aktif per cycle | ~15–20 detik |
| **Total per hari (144 cycle)** | **~200 mAh/hari** (dengan margin 50%) |

Panel surya 5–10W di kondisi tropis menghasilkan 1.000–2.000 mAh/hari, jauh lebih dari cukup. Dengan 2x baterai 18650 paralel (~4.000–6.000 mAh), device masih bisa jalan 3–5 hari saat mendung berturut-turut.

---

## Utang Teknis

Yang perlu diselesaikan sebelum mulai Proyek 3:

- [ ] **Migrasi modul ke shared library:** `power_mgmt`, `alerts`, `storage` masih lokal di `src/`. Perlu dipindah ke `lib/` (shared library monorepo) biar Proyek 3 tidak duplikasi kode yang sama.
- [ ] **Client certificate (mTLS):** saat ini pakai username/password. Perlu upgrade ke mutual TLS untuk deployment multi-device.
- [ ] **Verifikasi checksum firmware OTA:** OTA sudah jalan, tapi validasi signature/checksum binary sebelum flash belum diimplementasi.
- [ ] **Kalibrasi sensor:** wajib dilakukan dengan larutan referensi sebelum deployment. Nilai kalibrasi saat ini masih placeholder.

---

## Lisensi

[MIT License](LICENSE), bebas digunakan, dimodifikasi, dan didistribusikan dengan atribusi.
