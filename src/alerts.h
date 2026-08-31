#ifndef ALERTS_H
#define ALERTS_H

#include <Arduino.h>
#include "sensors.h"

// Pinout indikator
#define PIN_BUZZER     25
#define PIN_LED_RED    26
#define PIN_LED_YELLOW 27
#define PIN_LED_GREEN  14

// Ambang batas kualitas air (WHO Standard untuk air minum)
#define THRESHOLD_TDS_HIGH         500.0f  // ppm  — WHO max 600 ppm
#define THRESHOLD_TURBIDITY_HIGH    25.0f  // NTU  — WHO max 5 NTU (25 untuk alert)
#define THRESHOLD_PH_LOW             6.5f  // pH
#define THRESHOLD_PH_HIGH            8.5f  // pH

void initAlerts();
bool checkThresholds(SensorData data);
void setAlertState(bool isAlert);
void updateDisplay(SensorData data, bool isAlert);
void silenceAlerts();   // Matikan buzzer & LED sebelum deep sleep

#endif // ALERTS_H
