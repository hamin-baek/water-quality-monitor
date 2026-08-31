#include "sensors.h"

void initSensors() {
    analogSetAttenuation(ADC_11db);
    pinMode(PIN_TDS, INPUT);
    pinMode(PIN_TURBIDITY, INPUT);
    pinMode(PIN_PH, INPUT);
    Serial.println("Sensors initialized.");
}

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

    data.tds = rawTds * (1000.0f / 4095.0f);

    float turbVoltage = (rawTurbidity / 4095.0f) * 3.3f;
    float ntu = -1120.4f * turbVoltage * turbVoltage + 5742.3f * turbVoltage - 4352.9f;
    data.turbidity = max(0.0f, ntu);

    float phVoltage = (rawPh / 4095.0f) * 3.3f;
    data.ph = 7.0f + ((2.5f - phVoltage) / 0.18f);
    data.ph = constrain(data.ph, 0.0f, 14.0f);

    if (rawTds == 0 && rawTurbidity == 0 && rawPh == 0) {
        data.isError = true;
        Serial.println("SENSOR ERROR: All ADC pins reading 0. Check sensor connections!");
    }

    Serial.printf("[Sensors] TDS raw=%lu (%.1f ppm) | Turb raw=%lu (%.1f NTU) | pH raw=%lu (%.2f)\n",
                  rawTds, data.tds, rawTurbidity, data.turbidity, rawPh, data.ph);

    return data;
}
