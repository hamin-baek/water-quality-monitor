#ifndef POWER_MGMT_H
#define POWER_MGMT_H

#include <Arduino.h>

// MOSFET gate untuk menyalakan/mematikan sensor rail
// Gunakan GPIO yang tidak terpakai WiFi dan aman di semua mode
#define PIN_SENSOR_POWER_MOSFET 12

// Pin ADC untuk voltage divider baterai
// GPIO 34 adalah ADC1 — aman dipakai saat WiFi aktif
#define PIN_BATTERY_VOLTAGE 34

// Konstanta voltage divider baterai: asumsi R1=100k, R2=100k => faktor 2x
// Sesuaikan jika menggunakan nilai resistor berbeda
#define VBATT_DIVIDER_RATIO 2.0f
#define VBATT_ADC_REF       3.3f
#define VBATT_ADC_MAX       4095.0f

void initPowerMgmt();
void powerOnSensors();
void powerOffSensors();
float readBatteryVoltage();
void goToDeepSleep(uint64_t sleepTimeSeconds);

#endif // POWER_MGMT_H
