#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include "sensors.h"

#define PIN_SD_CS 5 // Chip Select untuk SD Card

void initStorage();
void logDataToSD(SensorData data, float batteryVoltage, bool isAlert, bool sentToCloud);
uint32_t getCurrentTimestamp();

// Sinkronisasi baris CSV yang belum terkirim ke cloud (sent_to_cloud == 0)
void syncOfflineData();

#endif // STORAGE_H
