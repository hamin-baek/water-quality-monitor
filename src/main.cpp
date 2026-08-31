#include <Arduino.h>
#include <esp_task_wdt.h>
#include "sensors.h"
#include "alerts.h"
#include "storage.h"
#include "connectivity.h"
#include "power_mgmt.h"

#define SLEEP_TIME_SECONDS  600ULL
#define WDT_TIMEOUT_SECONDS  90

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n========================================");
    Serial.println("  Water Quality Monitor  v1.0");
    Serial.println("========================================\n");

    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
    esp_task_wdt_add(NULL);
    Serial.printf("Watchdog active (%d second timeout).\n", WDT_TIMEOUT_SECONDS);

    initPowerMgmt();
    initSensors();
    initAlerts();
    initStorage();
    initConnectivity();
}

void loop() {
    esp_task_wdt_reset();

    Serial.println("\n--- New Reading Cycle ---");

    powerOnSensors();
    esp_task_wdt_reset();

    SensorData data      = readSensors();
    float batteryVoltage = readBatteryVoltage();

    powerOffSensors();

    bool isAlert = checkThresholds(data);
    setAlertState(isAlert);
    updateDisplay(data, isAlert);

    Serial.printf("Status: TDS=%.1f ppm | Turb=%.1f NTU | pH=%.2f | Batt=%.2fV | Alert=%s\n",
                  data.tds, data.turbidity, data.ph, batteryVoltage,
                  isAlert ? "YES" : "NO");

    esp_task_wdt_reset();

    uint32_t ts         = getCurrentTimestamp();
    bool sentToCloud    = publishTelemetry(data, batteryVoltage, ts);

    if (isAlert && sentToCloud) {
        publishAlert(data, "threshold_violation", ts);
    }

    if (sentToCloud) {
        esp_task_wdt_reset();
        syncOfflineData();

        esp_task_wdt_reset();
        checkForUpdates();
    }

    logDataToSD(data, batteryVoltage, isAlert, sentToCloud);

    silenceAlerts();
    disconnectConnectivity();

    Serial.printf("Cycle complete. Sleeping %llu seconds...\n\n", SLEEP_TIME_SECONDS);

    goToDeepSleep(SLEEP_TIME_SECONDS);
}
