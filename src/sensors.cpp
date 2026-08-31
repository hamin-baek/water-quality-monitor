#include "sensors.h"

void initSensors() {
    // ADC1 pins tidak perlu dikonfigurasi secara eksplisit,
    // tapi kita set attenuation ke 11dB untuk membaca 0-3.3V penuh
    analogSetAttenuation(ADC_11db);
    
    pinMode(PIN_TDS, INPUT);
    pinMode(PIN_TURBIDITY, INPUT);
    pinMode(PIN_PH, INPUT);
    
    Serial.println("Sensors initialized.");
}

// Fungsi helper: baca ADC dengan rata-rata N sampel untuk noise-filtering
static uint32_t readADCAverage(uint8_t pin) {
    uint32_t sum = 0;
    for (int i = 0; i < SENSOR_SAMPLE_COUNT; i++) {
        sum += analogRead(pin);
        delay(SENSOR_SAMPLE_DELAY_MS);
    }
    return sum / SENSOR_SAMPLE_COUNT;
}

SensorData readSensors() {
    SensorData data;
    data.isError = false;

    uint32_t rawTds      = readADCAverage(PIN_TDS);
    uint32_t rawTurbidity = readADCAverage(PIN_TURBIDITY);
    uint32_t rawPh       = readADCAverage(PIN_PH);

    // Konversi TDS: 0-4095 -> 0-1000 ppm (linear, kalibrasi sensor spesifik diperlukan)
    data.tds = rawTds * (1000.0f / 4095.0f);

    // Konversi Turbidity: NTU berbanding terbalik dengan voltase pada sensor SEN0189
    // Voltase tinggi = air jernih (NTU rendah), voltase rendah = air keruh (NTU tinggi)
    float turbVoltage = (rawTurbidity / 4095.0f) * 3.3f;
    // Formula empiris: NTU = -1120.4 * V^2 + 5742.3 * V - 4352.9 (dari datasheet SEN0189)
    // Clamp ke 0 untuk menghindari nilai negatif
    float ntu = -1120.4f * turbVoltage * turbVoltage + 5742.3f * turbVoltage - 4352.9f;
    data.turbidity = max(0.0f, ntu);

    // Konversi pH: sensor analog pH menghasilkan ~2.5V @ pH 7 (mid-point)
    // dengan slope ~-0.18V/pH (variasi per sensor, perlu kalibrasi)
    float phVoltage = (rawPh / 4095.0f) * 3.3f;
    data.ph = 7.0f + ((2.5f - phVoltage) / 0.18f);
    data.ph = constrain(data.ph, 0.0f, 14.0f);

    // Validasi: nilai di luar range fisik menandakan sensor tidak terhubung / rusak
    if (rawTds == 0 && rawTurbidity == 0 && rawPh == 0) {
        data.isError = true;
        Serial.println("SENSOR ERROR: Semua pin ADC membaca 0. Cek koneksi sensor!");
    }

    Serial.printf("[Sensors] TDS raw=%lu (%.1f ppm) | Turb raw=%lu (%.1f NTU) | pH raw=%lu (%.2f)\n",
                  rawTds, data.tds, rawTurbidity, data.turbidity, rawPh, data.ph);

    return data;
}
