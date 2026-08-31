#include "power_mgmt.h"

void initPowerMgmt() {
    pinMode(PIN_SENSOR_POWER_MOSFET, OUTPUT);
    digitalWrite(PIN_SENSOR_POWER_MOSFET, LOW);
    analogSetPinAttenuation(PIN_BATTERY_VOLTAGE, ADC_11db);
    Serial.println("Power management initialized.");
}

void powerOnSensors() {
    digitalWrite(PIN_SENSOR_POWER_MOSFET, HIGH);
    delay(500);
    Serial.println("Sensor power ON.");
}

void powerOffSensors() {
    digitalWrite(PIN_SENSOR_POWER_MOSFET, LOW);
    Serial.println("Sensor power OFF.");
}

float readBatteryVoltage() {
    uint32_t sum = 0;
    for (int i = 0; i < 5; i++) {
        sum += analogRead(PIN_BATTERY_VOLTAGE);
        delay(2);
    }
    float rawAvg = (float)(sum / 5);

    float vout  = (rawAvg / VBATT_ADC_MAX) * VBATT_ADC_REF;
    float vbatt = vout * VBATT_DIVIDER_RATIO;

    vbatt = constrain(vbatt, 0.0f, 5.0f);

    Serial.printf("Battery: raw=%.0f, Vout=%.3fV, Vbatt=%.3fV\n", rawAvg, vout, vbatt);
    return vbatt;
}

void goToDeepSleep(uint64_t sleepTimeSeconds) {
    Serial.printf("Entering Deep Sleep for %llu seconds...\n", sleepTimeSeconds);
    Serial.flush();
    
    esp_sleep_enable_timer_wakeup(sleepTimeSeconds * 1000000ULL);
    esp_deep_sleep_start();
}
