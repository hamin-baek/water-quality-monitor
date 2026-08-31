#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

// Deklarasi pin sensor (ADC-only pins — aman untuk ESP32)
#define PIN_TDS        35  // ADC1_CH7
#define PIN_TURBIDITY  32  // ADC1_CH4
#define PIN_PH         33  // ADC1_CH5

// Jumlah sampel untuk noise-filtering (rata-rata)
#define SENSOR_SAMPLE_COUNT 10
#define SENSOR_SAMPLE_DELAY_MS 5

// Struktur untuk menyimpan hasil pembacaan
struct SensorData {
    float tds;       // dalam ppm
    float turbidity; // dalam NTU
    float ph;        // nilai pH (0-14)
    bool isError;    // true jika sensor rusak / bacaan tidak valid
};

void initSensors();
SensorData readSensors();

#endif // SENSORS_H
