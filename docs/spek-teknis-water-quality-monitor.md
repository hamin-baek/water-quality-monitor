# Spesifikasi Teknis — Water Quality Monitor (Proyek 1)

**Konfigurasi yang dipilih:** Terhubung cloud (WiFi ke server + dashboard) · Power: Baterai + Solar Panel (lokasi tanpa akses listrik)

Karena kamu pilih cloud-connected + solar, ini mengubah beberapa asumsi dari panduan awal. Perbedaan paling besar: **konsumsi daya jadi prioritas desain nomor satu**, dan kita butuh strategi sleep/wake yang ketat supaya baterai tidak habis sebelum solar sempat mengisi ulang.

---

## 1. Device Requirements Document (DRD)

### 1.1 Sensor & Aktuator

| Komponen | Tipe | Fungsi |
|---|---|---|
| Sensor TDS | Analog | Ukur total dissolved solids (ppm) |
| Sensor Turbidity | Analog | Ukur kekeruhan air (NTU) |
| Sensor pH | Analog | Ukur keasaman air |
| LCD 16x2 I2C | Aktuator/display | Tampilan lokal untuk warga (baca angka langsung di lokasi) |
| Buzzer aktif | Aktuator | Alert suara lokal saat threshold dilanggar |
| LED indikator (merah/kuning/hijau) | Aktuator | Indikator visual cepat status air |
| Modul SD card | Storage lokal | Backup log kalau WiFi/cloud tidak tersedia |
| RTC DS3231 | Timekeeping | Timestamp akurat meski device sleep/restart |
| Fuel gauge / voltage divider baterai | Sensing | Monitor level baterai (kirim ke cloud, untuk tahu kapan perlu maintenance) |

### 1.2 Sampling Rate

Kualitas air tidak berubah drastis dalam hitungan detik, jadi **tidak perlu continuous sampling**. Dengan constraint baterai+solar, sampling rate justru harus dijaga serendah mungkin yang masih aman secara keselamatan warga.

- **Sampling interval: setiap 10 menit** (144x/hari) — cukup responsif untuk isu kualitas air, cukup hemat baterai.
- **Local alert check: setiap kali sampling** — evaluasi threshold terjadi di tiap wake cycle, tidak menunggu kirim ke cloud.
- Kalau nanti setelah deployment ternyata baterai terlalu boros atau sebaliknya terlalu longgar, interval ini adalah parameter pertama yang di-tuning (naikkan ke 15-30 menit kalau boros; turunkan ke 5 menit kalau warga butuh data lebih real-time dan baterai ternyata kuat).

### 1.3 Real-time vs Batch

**Hybrid — bukan murni real-time, bukan murni batch:**

- **Event-driven (mendekati real-time) untuk alert**: begitu sensor mendeteksi pelanggaran threshold, buzzer/LED lokal langsung menyala saat itu juga (tidak menunggu jadwal), dan device akan mencoba kirim alert ke cloud secepatnya di cycle itu juga — ini prioritas tinggi, boleh "bangunkan" WiFi lebih awal dari jadwal normal kalau perlu.
- **Periodic/batch untuk data tren normal**: data rutin (bukan alert) dikirim tiap 10 menit sesuai sampling interval, bukan streaming terus-menerus — ini menghemat baterai signifikan dibanding koneksi WiFi yang selalu nyala.
- Kalau koneksi WiFi/cloud gagal saat sampling, data disimpan dulu (SD card + buffer di memori) dan dikirim sebagai batch saat koneksi berikutnya berhasil (store-and-forward).

---

## 2. Bill of Materials (BOM)

BOM ini sudah disesuaikan untuk versi cloud-connected + solar (menambah komponen power management, tidak menambah radio module karena ESP32 sudah punya WiFi built-in).

| Komponen | Fungsi | Estimasi Harga |
|---|---|---|
| ESP32 DevKit V1 (WiFi built-in) | Otak sistem + konektivitas | Rp50.000 |
| Sensor TDS + modul konverter | Ukur TDS | Rp75.000–120.000 |
| Sensor Turbidity | Ukur kekeruhan | Rp60.000–90.000 |
| Sensor pH + probe | Ukur pH | Rp90.000–150.000 |
| LCD 16x2 I2C | Display lokal | Rp30.000 |
| Buzzer aktif | Alert suara | Rp5.000 |
| LED merah/kuning/hijau + resistor | Indikator visual | Rp5.000 |
| Modul Micro SD + kartu 8GB | Backup log lokal | Rp30.000 |
| RTC DS3231 | Timestamp | Rp20.000 |
| Panel surya 5-10W (12V atau 6V tergantung charge controller) | Sumber energi utama | Rp60.000–100.000 |
| Solar charge controller (misal CN3065/TP4056 dengan proteksi over-charge/discharge, atau modul solar charger khusus seperti Adafruit/generic MPPT kecil) | Kontrol pengisian baterai dari solar | Rp30.000–70.000 |
| Baterai 18650 Li-ion (2x, atau 1x LiFePO4 kapasitas lebih besar) | Penyimpan energi | Rp40.000–90.000 |
| Modul boost/buck converter (kalau tegangan baterai tidak pas 3.3-5V yang dibutuhkan ESP32 & sensor) | Stabilkan tegangan | Rp15.000–30.000 |
| MOSFET logic-level (misal IRLZ44N) + resistor, untuk power-switching sensor rail | Matikan power ke sensor saat sleep (hemat baterai) | Rp10.000–20.000 |
| Box IP65 outdoor + gland kabel + lem silikon | Enclosure tahan air/debu | Rp60.000–90.000 |
| Kabel, konektor, mounting bracket | Instalasi | Rp30.000–50.000 |

**Total estimasi: Rp610.000–950.000 per unit** — naik dari target awal 400rb karena tambahan solar+baterai+power management. Ini trade-off yang wajar untuk lokasi tanpa listrik + fitur cloud dashboard. Kalau budget jadi kendala, opsi pertama yang bisa dikurangi: pakai 1 baterai 18650 saja (bukan 2) untuk pilot pertama, atau turunkan spek panel surya ke 5W dulu.

---

## 3. System Architecture (3 Layer)

```
┌─────────────────────────┐     WiFi (MQTT/TLS)     ┌──────────────────────────┐
│      DEVICE LAYER        │ ───────────────────────▶│    CLOUD/BACKEND LAYER    │
│                           │                          │                            │
│  ESP32 + sensor TDS/      │                          │  MQTT Broker (TLS)         │
│  Turbidity/pH + LCD +     │◀─────────────────────── │  → Backend/Processor       │
│  Buzzer/LED + SD + RTC    │   (command/OTA trigger)  │  → Database (time-series)  │
│  + solar/baterai          │                          │  → Dashboard web           │
└─────────────────────────┘                          │  → Alerting (WA/Telegram    │
                                                        │     ke kecamatan)          │
                                                        └──────────────────────────┘
```

**Catatan penting soal "gateway/edge layer":** Karena ESP32 sudah punya WiFi built-in, di desain ini **tidak ada hardware gateway terpisah** — ESP32 itu sendiri berperan ganda sebagai *device* sekaligus *edge/gateway* (langsung konek WiFi ke internet, tidak lewat perantara). Ini beda dengan Proyek 3 (Forest Fire) yang nanti butuh gateway fisik terpisah karena node-nya pakai LoRa (bukan WiFi) dan perlu satu titik yang menjembatani LoRa ke internet.

Yang membuat ESP32 di sini tetap berfungsi sebagai "edge" secara logis (bukan cuma dumb sensor forwarder) adalah: **semua logika safety-critical (cek threshold, nyalakan buzzer/LED) berjalan lokal di device, tidak bergantung pada cloud.** Kalau WiFi mati/cloud down, warga tetap dapat alert dari buzzer & LCD. Cloud hanya untuk data historis, dashboard, dan alert tambahan ke kecamatan.

### Layer breakdown:

**Device Layer** (ESP32 firmware):
- Baca sensor, evaluasi threshold, kontrol buzzer/LED/LCD
- Log lokal ke SD card
- Kelola power (sleep/wake, matikan sensor rail saat idle)
- Kirim data ke cloud via MQTT saat terjaga

**Edge logic** (built-in di firmware ESP32, bukan perangkat terpisah):
- Filtering noise sensor (rata-rata beberapa pembacaan)
- Keputusan alert lokal (tidak menunggu respons cloud)
- Antrian offline (simpan data kalau gagal kirim, kirim ulang nanti)

**Cloud/Backend Layer:**
- **MQTT Broker** — terima data dari semua device (bisa banyak titik air nantinya)
- **Backend/processor** — subscribe topic MQTT, validasi data, simpan ke database, jalankan logika alert tambahan (misal kirim WhatsApp/Telegram ke kecamatan)
- **Database** — time-series storage untuk histori data tiap device
- **Dashboard** — web app untuk kecamatan/dinas kesehatan pantau semua titik air

**Rekomendasi stack cloud untuk pemula (2 opsi):**

| | Opsi A — MVP cepat | Opsi B — Self-hosted, scalable |
|---|---|---|
| Cocok untuk | Pilot 1 device, belajar cepat | Kalau nanti mau scale ke banyak device/desa (termasuk proyek 2 & 3) |
| Broker+Dashboard | **Adafruit IO** (gratis, MQTT ready, ada dashboard widget siap pakai) | Mosquitto (MQTT broker) di VPS murah |
| Processing | Built-in di Adafruit IO | **Node-RED** (visual programming, gampang untuk pemula, powerful untuk logic alert) |
| Database | Built-in Adafruit IO (retensi terbatas di free tier) | **InfluxDB** (time-series) |
| Dashboard | Adafruit IO dashboard bawaan | **Grafana** (dashboard time-series yang bagus) |
| Biaya | Gratis (dengan limit) | ~Rp50.000-100.000/bulan VPS (misal 1-2GB RAM) |

**Saran saya:** mulai dengan **Opsi A (Adafruit IO)** untuk pilot pertama ini — supaya kamu bisa fokus belajar firmware dulu tanpa pusing setup server. Begitu proyek 1 jalan stabil dan kamu mulai pikirkan Proyek 2 & 3 (yang butuh banyak device), baru migrasi ke **Opsi B** karena satu stack itu bisa dipakai bersama untuk ketiga proyek sekaligus.

---

## 4. Connectivity & Protocol Plan

### 4.1 Jaringan: WiFi
- ESP32 pakai WiFi 2.4GHz built-in — tidak perlu modul tambahan.
- **Wajib survey sinyal WiFi di lokasi pemasangan sebelum deployment.** Titik air (tandon/keran) sering di luar jangkauan router rumah warga. Cek dengan HP di lokasi persis pemasangan.
- Kalau sinyal lemah: opsi mudah pasang **WiFi range extender/repeater** di titik antara; kalau tidak ada WiFi sama sekali di area itu, ini jadi blocker besar untuk desain cloud-connected — perlu dipertimbangkan ulang pakai GSM (seperti proyek 2) sebagai gantinya untuk titik spesifik itu.

### 4.2 Protokol: MQTT (bukan HTTP)
**MQTT dipilih karena:**
- Lebih hemat daya & bandwidth dibanding HTTP (penting untuk device baterai)
- Pub/sub cocok untuk multi-device ke satu dashboard
- Ada fitur **Last Will and Testament (LWT)** — broker otomatis bisa tandai device "offline" kalau device mati mendadak/baterai habis, berguna untuk maintenance
- QoS control untuk memastikan data terkirim

**Detail:**
- Transport: **MQTT over TLS (MQTTS)**, port **8883** (bukan port 1883 plaintext)
- QoS level: **QoS 1** (at least once) — cukup untuk data sensor, tidak perlu overhead QoS 2
- Topic structure:
  - `desa/{nama_desa}/waterquality/{device_id}/telemetry` — data rutin
  - `desa/{nama_desa}/waterquality/{device_id}/alert` — alert event (threshold dilanggar)
  - `desa/{nama_desa}/waterquality/{device_id}/status` — online/offline (pakai LWT)
  - `desa/{nama_desa}/waterquality/{device_id}/cmd` — command dari cloud ke device (misal trigger OTA check)

### 4.3 Kapan device connect
- **Bukan persistent connection 24 jam** (terlalu boros baterai). Device connect WiFi+MQTT hanya saat wake cycle (tiap 10 menit), publish data, lalu disconnect & deep sleep.
- Exception: kalau alert terdeteksi, device tetap connect di cycle itu juga (tidak menunggu next scheduled cycle) untuk kirim alert secepatnya.

---

## 5. Data Schema

### 5.1 Payload MQTT (JSON, topik `telemetry`)

```json
{
  "ts": 1735459200,
  "tds": 245.3,
  "turb": 3.2,
  "ph": 7.1,
  "batt_v": 3.87,
  "alert": false
}
```

- `device_id` **tidak** perlu dimasukkan ke payload karena sudah ada di topic MQTT-nya — ini menghemat bandwidth/energi kirim.
- Nama field disingkat (`tds`, `turb`, `batt_v`) untuk memperkecil ukuran payload — total payload ini sekitar **90-110 bytes**, sangat kecil, tidak jadi masalah untuk WiFi (beda dengan LoRa di proyek 3 yang payload-nya harus jauh lebih ketat).
- `ts` pakai Unix timestamp dari RTC (bukan dari NTP/cloud) supaya tetap akurat meski WiFi delay/gagal connect.

### 5.2 Payload alert (topik `alert`)

```json
{
  "ts": 1735459200,
  "reason": "tds_high",
  "value": 620.5,
  "threshold": 500
}
```

### 5.3 Skema log lokal di SD card (CSV, untuk backup & audit)

```
timestamp,tds_ppm,turbidity_ntu,ph,battery_v,alert_flag,sent_to_cloud
2026-08-29T14:10:00,245.3,3.2,7.1,3.87,0,1
```

Kolom `sent_to_cloud` (0/1) dipakai firmware untuk tahu baris mana yang masih perlu dikirim ulang kalau sempat gagal kirim.

---

## 6. Power Budget

Ini bagian paling kritis untuk desain baterai+solar — kalau salah hitung, device bisa mati di tengah jalan sebelum sempat charge lagi.

### 6.1 Estimasi konsumsi arus per komponen

| Komponen | Kondisi | Estimasi arus |
|---|---|---|
| ESP32 | Deep sleep | 10-150 µA |
| ESP32 | Active + WiFi connect/transmit | 150-250 mA (bisa spike ke 350-400mA sesaat) |
| Sensor TDS/Turbidity/pH (gabungan 3 sensor) | Aktif (powered via MOSFET switch) | ~30-60 mA total |
| LCD I2C + backlight | Menyala | 20-30 mA |
| Buzzer | Aktif (hanya saat alert) | 20-30 mA |
| SD card module | Menulis | 10-100 mA (spike saat write) |
| RTC DS3231 | Selalu on (baterai coin cell sendiri, tidak bebani baterai utama) | ~1-3 µA dari baterai utama (idle) |

### 6.2 Contoh perhitungan (sampling tiap 10 menit)

**Asumsi per wake cycle:**
- Durasi aktif total (bangun → baca sensor → evaluasi → connect WiFi → publish MQTT → sleep lagi): **~15-20 detik**
- Arus rata-rata selama aktif (WiFi connect + sensor + kadang LCD/SD menyala bersamaan): **~180 mA**
- Sisa waktu 1 cycle (10 menit = 600 detik) device di deep sleep: **~580-585 detik @ ~0.15 mA**

**Energi per cycle:**
- Aktif: 180 mA × 18s ÷ 3600 = **0.9 mAh**
- Sleep: 0.15 mA × 582s ÷ 3600 = **0.024 mAh**
- Total per cycle ≈ **0.92 mAh**

**Per hari (144 cycle):**
- 0.92 mAh × 144 = **~133 mAh/hari** (belum termasuk overhead alert/buzzer yang jarang terjadi, tambahkan margin)

**Dengan margin keamanan 50% (untuk hari mendung, alert lebih sering, LCD nyala lebih lama, dll):** perkiraan realistis **~200 mAh/hari**.

### 6.3 Sizing baterai & solar panel

- **Baterai:** dengan konsumsi ~200 mAh/hari, baterai 18650 tunggal (2000-3000mAh) **tanpa solar sama sekali** sebenarnya bisa tahan 10-15 hari. Tapi karena kamu pilih solar, ini artinya sistem punya buffer besar untuk hari-hari mendung berturut-turut (misal 3-5 hari mendung tetap aman) — rekomendasi: pakai **2x baterai 18650 paralel (total ~4000-6000mAh)** untuk buffer ekstra, terutama karena ini device kritis (kualitas air) yang tidak boleh mati total.
- **Solar panel:** panel 5-10W di kondisi cerah tropis Kalimantan Timur (rata-rata 4-5 jam sun-hours efektif/hari) bisa hasilkan ~1000-2000mAh/hari (tergantung efisiensi charge controller) — jauh lebih dari cukup untuk kebutuhan 200mAh/hari, dengan banyak margin untuk musim hujan/mendung.
- **Charge controller wajib punya proteksi over-charge & over-discharge** — baterai Li-ion yang di-charge/discharge tanpa proteksi berisiko rusak cepat atau bahkan berbahaya (panas berlebih).

### 6.4 Implikasi desain firmware
Power budget ini **menentukan arsitektur firmware** — lihat Bagian 7. Intinya: device HARUS deep sleep sebagian besar waktu, sensor HARUS dimatikan (lewat MOSFET) saat tidak dipakai, dan koneksi WiFi/MQTT HARUS singkat (connect-publish-disconnect), bukan persistent connection.

---

## 7. Firmware Architecture

### 7.1 State machine per wake cycle

```
[DEEP SLEEP - ditrigger RTC timer, interval 10 menit]
        │
        ▼
   [BOOT / WAKE]
        │
        ▼
  [INIT peripherals: I2C, SPI, RTC baca waktu]
        │
        ▼
  [POWER ON sensor rail via MOSFET] → tunggu stabilisasi (beberapa ratus ms - detik, cek datasheet tiap sensor)
        │
        ▼
  [BACA SENSOR: ambil beberapa sample, rata-ratakan (noise filtering)]
        │
        ▼
  [POWER OFF sensor rail]  ← matikan segera setelah selesai baca, jangan tunggu
        │
        ▼
  [EVALUASI THRESHOLD LOKAL] → kalau melanggar: NYALAKAN BUZZER + LED (fail-safe, TIDAK bergantung cloud)
        │
        ▼
  [TAMPILKAN ke LCD] (opsional: matikan LCD lebih cepat kalau mau hemat, atau nyala beberapa detik saja)
        │
        ▼
  [LOG ke SD card] (selalu, terlepas dari status koneksi)
        │
        ▼
  [CONNECT WiFi] (timeout max 8-10 detik, jangan infinite retry)
        │
    ┌───┴────┐
  sukses    gagal
    │          │
    ▼          ▼
[CONNECT MQTT   [SIMPAN data ke antrian
 broker TLS]     lokal (RTC memory/SD),
    │            tandai belum terkirim]
    ▼                    │
[PUBLISH telemetry        │
 (+ antrian data lama      │
 yang belum terkirim,       │
 kalau ada)]                │
    │                       │
    ▼                       │
[Kalau alert: publish        │
 topic alert juga]           │
    │                       │
    ▼                       ▼
[DISCONNECT WiFi/MQTT]  [langsung lanjut]
        │
        ▼
  [DEEP SLEEP lagi sampai next interval]
```

### 7.2 Watchdog Timer
- **Wajib aktifkan hardware watchdog ESP32** (`esp_task_wdt`). Kalau firmware hang (misal stuck di WiFi connect loop karena bug), watchdog akan otomatis restart device. Tanpa ini, device yang macet di lokasi terpencil bisa mati total sampai ada orang datang manual reset — sangat mahal secara operasional untuk device yang dipasang jauh.
- Set watchdog timeout lebih besar dari waktu maksimum yang wajar untuk satu wake cycle (misal kalau cycle normal 20 detik, set watchdog 30-60 detik).

### 7.3 Reconnect & Retry Logic
- WiFi connect: retry dengan **exponential backoff**, maksimal 3-4x percobaan per cycle, lalu **give up untuk cycle ini** dan coba lagi di cycle berikutnya (jangan retry tanpa henti — ini yang paling menguras baterai kalau WiFi sedang bermasalah).
- MQTT publish gagal: data disimpan ke antrian lokal (bisa pakai **RTC memory** ESP32 yang bertahan across deep sleep untuk antrian kecil, atau SD card untuk antrian lebih besar/lebih tahan lama), dikirim ulang di cycle berikutnya yang berhasil connect.
- Alert tetap tervalidasi & buzzer tetap menyala **meskipun WiFi/cloud gagal total** — ini prinsip fail-safe paling penting di seluruh desain ini.

### 7.4 Error Handling Sensor
- Kalau pembacaan sensor di luar rentang wajar (misal pH < 0 atau > 14, atau nilai sensor terputus/short circuit) → jangan langsung anggap "alert bahaya", tandai sebagai **`sensor_error`** terpisah dari alert kualitas air asli. Ini mencegah false alarm ke warga karena sensor rusak/kotor, bukan air yang benar-benar bermasalah.
- Kirim status `sensor_error` ke cloud supaya petugas tahu perlu cek/bersihkan sensor.

### 7.5 Struktur kode (modular)
Sarankan pisah kode jadi beberapa file/module (bukan 1 file besar) supaya gampang di-maintain:
- `sensors.cpp/.h` — baca & kalibrasi TDS/turbidity/pH
- `power_mgmt.cpp/.h` — kontrol MOSFET sensor rail, deep sleep, baca level baterai
- `connectivity.cpp/.h` — WiFi connect, MQTT publish/subscribe, TLS setup
- `storage.cpp/.h` — SD card log, antrian offline
- `alerts.cpp/.h` — evaluasi threshold, kontrol buzzer/LED/LCD
- `main.cpp` — orchestrate state machine di atas

---

## 8. Security Plan

### 8.1 Autentikasi device ke server
- **Level pilot (rekomendasi untuk mulai):** username/password unik per device untuk login ke MQTT broker (semua broker seperti Mosquitto/Adafruit IO/EMQX support ini). Simpan credential ini **tidak di-hardcode di source code yang mungkin ke-push ke GitHub public** — pisahkan ke file konfigurasi terpisah (`secrets.h` yang di-gitignore) atau simpan di NVS (non-volatile storage) ESP32 lewat proses provisioning terpisah.
- **Level lebih matang (kalau sudah scale ke banyak device):** client certificate per device (mutual TLS) — lebih aman karena tidak ada password yang bisa dicuri/di-share, tapi setup lebih rumit (perlu certificate authority, provisioning tiap device dengan cert unik). Ini upgrade yang masuk akal setelah pilot 1 device sukses dan kamu mulai deploy ke banyak titik.

### 8.2 Enkripsi koneksi
- **TLS wajib** — semua komunikasi device↔broker lewat MQTT over TLS (port 8883), bukan plaintext MQTT (port 1883). Data kualitas air memang bukan data super rahasia, tapi tanpa TLS, siapapun di jaringan yang sama bisa **menyuntik data palsu** (misal kirim data "aman" palsu untuk menutupi air yang sebenarnya tercemar) — makanya integritas data ini penting justru karena dipakai untuk keselamatan publik.
- ESP32 (chip modern, cukup RAM) sanggup handle TLS handshake tanpa masalah performa berarti.

### 8.3 OTA (Over-The-Air) Firmware Update
Karena device dipasang di lokasi yang tidak selalu gampang diakses fisik, OTA penting untuk maintenance jangka panjang (perbaikan bug, update threshold, dll) tanpa harus datang ke lokasi.

- **Metode: HTTPS-based OTA** (bukan `ArduinoOTA` biasa yang cuma jalan di jaringan lokal yang sama) — device secara berkala (misal 1x/hari, bukan tiap cycle, karena OTA check juga makan baterai & data) cek endpoint HTTPS ke server untuk versi firmware terbaru.
- Firmware binary yang di-download **harus diverifikasi** (checksum/signature) sebelum di-flash, supaya device tidak menerima firmware yang corrupt atau (secara teori) firmware jahat yang disusupkan lewat MITM kalau ada celah.
- Trigger OTA check **bisa juga lewat MQTT command** (`.../cmd` topic) supaya kamu bisa push update kapan saja tanpa nunggu jadwal, device akan cek saat wake cycle berikutnya.
- Gunakan ESP32 `Update` library (built-in) dengan skema **dual partition (OTA_0/OTA_1)** supaya kalau update gagal di tengah jalan, device tetap bisa boot ke firmware lama (fail-safe, tidak jadi "brick").

### 8.4 Checklist keamanan minimum untuk pilot ini
- [x] MQTT over TLS (port 8883)
- [x] Username/password unik per device (bukan shared credential semua device)
- [x] Credential tidak di-hardcode di kode yang di-commit ke repo public
- [x] Validasi data di sisi backend (reject payload dengan nilai di luar rentang fisik wajar, misal pH negatif — mencegah data sampah/corrupt masuk database)
- [ ] Client certificate (mTLS) — upgrade untuk fase scale-up, tidak wajib di pilot
- [x] OTA dengan verifikasi firmware sebelum flash

---

## Ringkasan Perubahan dari Panduan Awal

Karena kamu pilih **cloud-connected + solar** (bukan standalone lokal murni), beberapa hal berubah dari estimasi awal:

| Aspek | Sebelumnya | Sekarang |
|---|---|---|
| Budget per unit | ~Rp350-450rb | ~Rp610-950rb (tambahan solar+baterai+power mgmt) |
| Kompleksitas firmware | Sederhana (loop baca-tampil-log) | Naik signifikan (state machine, sleep/wake, MQTT/TLS, OTA, watchdog) |
| Butuh survey lokasi tambahan | Tidak | Ya — cek sinyal WiFi & paparan sinar matahari untuk solar panel di titik pemasangan |
| Waktu pengerjaan estimasi | 3-5 minggu | Kemungkinan **5-8 minggu** karena kompleksitas power management + cloud stack |

Ini bukan berarti pilihan kamu salah — cloud dashboard untuk kecamatan itu value yang nyata. Tapi wajar untuk tahu bahwa ini menaikkan kompleksitas proyek pertama kamu cukup jauh dari desain paling sederhana. Kalau nanti di tengah jalan terasa kebanyakan sekaligus untuk proyek pertama, opsi realistis: **build dulu versi standalone-lokal (tanpa cloud) sampai jalan stabil, baru tambahkan layer WiFi/MQTT/cloud di iterasi kedua** — ini mengurangi jumlah hal baru yang harus dipelajari sekaligus.
