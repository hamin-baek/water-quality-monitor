#include "power_mgmt.h"

void initPowerMgmt() {
    pinMode(PIN_SENSOR_POWER_MOSFET, OUTPUT);
    digitalWrite(PIN_SENSOR_POWER_MOSFET, LOW); // Default: sensor rail mati
    
    // GPIO 34 adalah input-only, tidak perlu pinMode tapi kita set attenuation
    analogSetPinAttenuation(PIN_BATTERY_VOLTAGE, ADC_11db); // Range 0-3.3V
    
    Serial.println("Power management initialized.");
}

void powerOnSensors() {
    digitalWrite(PIN_SENSOR_POWER_MOSFET, HIGH);
    // Tunggu sensor stabil setelah dihidupkan (kapasitor decoupling, dll.)
    delay(500);
    Serial.println("Sensor power ON.");
}

void powerOffSensors() {
    digitalWrite(PIN_SENSOR_POWER_MOSFET, LOW);
    Serial.println("Sensor power OFF.");
}

float readBatteryVoltage() {
    // Rata-rata 5 sampel untuk stabilitas
    uint32_t sum = 0;
    for (int i = 0; i < 5; i++) {
        sum += analogRead(PIN_BATTERY_VOLTAGE);
        delay(2);
    }
    float rawAvg = (float)(sum / 5);

    // Konversi: raw ADC -> Vout -> Vbatt
    float vout  = (rawAvg / VBATT_ADC_MAX) * VBATT_ADC_REF;
    float vbatt = vout * VBATT_DIVIDER_RATIO;

    // Clamp: baterai LiPo 1S biasanya 3.0V - 4.2V, masih wajar sampai 5V
    vbatt = constrain(vbatt, 0.0f, 5.0f);

    Serial.printf("Battery: raw=%.0f, Vout=%.3fV, Vbatt=%.3fV\n", rawAvg, vout, vbatt);
    return vbatt;
}

void goToDeepSleep(uint64_t sleepTimeSeconds) {
    Serial.printf("Masuk Deep Sleep selama %llu detik...\n", sleepTimeSeconds);
    Serial.flush(); // Pastikan semua Serial output terkirim sebelum sleep
    
    // Konversi detik ke mikrodetik (perkalian 64-bit agar tidak overflow)
    esp_sleep_enable_timer_wakeup(sleepTimeSeconds * 1000000ULL);
    esp_deep_sleep_start();
    // Kode setelah baris ini tidak akan pernah dieksekusi
}
