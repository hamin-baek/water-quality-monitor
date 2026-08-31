#ifndef CONNECTIVITY_H
#define CONNECTIVITY_H

#include <Arduino.h>
#include "sensors.h"
#include <PubSubClient.h>

// Fungsi-fungsi terkait WiFi dan MQTT
void initConnectivity();
bool connectWiFi();
bool connectMQTT();
void disconnectConnectivity();

// Fungsi untuk mengirim data
bool publishTelemetry(SensorData data, float batteryVoltage, uint32_t timestamp);
bool publishAlert(SensorData data, const char* reason, uint32_t timestamp);

// Akses ke mqttClient dari modul lain (dibutuhkan oleh storage untuk syncOfflineData)
PubSubClient& getMqttClient();

// Over-The-Air (OTA) Updates
void checkForUpdates();

#endif // CONNECTIVITY_H
