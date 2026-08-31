#include "storage.h"
#include "connectivity.h"
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <ArduinoJson.h>

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #error "File secrets.h not found!"
#endif

static RTC_DS3231 rtc;
static bool sdAvailable  = false;
static bool rtcAvailable = false;

void initStorage() {
    if (!rtc.begin()) {
        Serial.println("WARNING: RTC not found! Timestamps will be 0.");
        rtcAvailable = false;
    } else {
        rtcAvailable = true;
        if (rtc.lostPower()) {
            Serial.println("RTC lost power. Setting to compile time.");
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        Serial.printf("RTC OK. Current time: %s\n", 
                      rtc.now().timestamp(DateTime::TIMESTAMP_FULL).c_str());
    }

    if (!SD.begin(PIN_SD_CS)) {
        Serial.println("WARNING: SD Card not found! Logging disabled.");
        sdAvailable = false;
    } else {
        Serial.println("SD Card OK.");
        sdAvailable = true;

        if (!SD.exists("/log.csv")) {
            File f = SD.open("/log.csv", FILE_WRITE);
            if (f) {
                f.println("timestamp,tds_ppm,turbidity_ntu,ph,battery_v,alert_flag,sent_to_cloud");
                f.close();
                Serial.println("New log.csv file created.");
            }
        }
    }
}

void logDataToSD(SensorData data, float batteryVoltage, bool isAlert, bool sentToCloud) {
    if (!sdAvailable) return;

    File file = SD.open("/log.csv", FILE_APPEND);
    if (!file) {
        Serial.println("ERROR: Failed to open log.csv for writing.");
        return;
    }

    char timestampStr[25];
    if (rtcAvailable) {
        DateTime now = rtc.now();
        snprintf(timestampStr, sizeof(timestampStr), "%04d-%02d-%02dT%02d:%02d:%02d",
                 now.year(), now.month(), now.day(),
                 now.hour(), now.minute(), now.second());
    } else {
        snprintf(timestampStr, sizeof(timestampStr), "1970-01-01T00:00:00");
    }

    file.printf("%s,%.1f,%.1f,%.2f,%.2f,%d,%d\n",
                timestampStr,
                data.tds,
                data.turbidity,
                data.ph,
                batteryVoltage,
                isAlert ? 1 : 0,
                sentToCloud ? 1 : 0);

    file.close();
    Serial.println("Data saved to SD Card.");
}

uint32_t getCurrentTimestamp() {
    if (rtcAvailable) {
        return rtc.now().unixtime();
    }
    return 0;
}

void syncOfflineData() {
    if (!sdAvailable) return;

    PubSubClient& mqtt = getMqttClient();
    if (!mqtt.connected()) return;
    File srcFile = SD.open("/log.csv", FILE_READ);
    if (!srcFile) {
        Serial.println("syncOfflineData: cannot open log.csv.");
        return;
    }

    SD.remove("/log_tmp.csv");
    File tmpFile = SD.open("/log_tmp.csv", FILE_WRITE);
    if (!tmpFile) {
        Serial.println("syncOfflineData: cannot create log_tmp.csv.");
        srcFile.close();
        return;
    }

    int totalUnsent  = 0;
    int totalSynced  = 0;
    bool isFirstLine = true;

    while (srcFile.available()) {
        String line = srcFile.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) continue;

        if (isFirstLine) {
            tmpFile.println(line);
            isFirstLine = false;
            continue;
        }

        bool alreadySent = line.endsWith(",1");

        if (alreadySent) {
            tmpFile.println(line);
            continue;
        }

        totalUnsent++;

        char buf[128];
        line.toCharArray(buf, sizeof(buf));

        char* tok_ts   = strtok(buf,  ",");
        char* tok_tds  = strtok(NULL, ",");
        char* tok_turb = strtok(NULL, ",");
        char* tok_ph   = strtok(NULL, ",");
        char* tok_batt = strtok(NULL, ",");
        char* tok_alrt = strtok(NULL, ",");

        if (!tok_ts || !tok_tds || !tok_turb || !tok_ph || !tok_batt || !tok_alrt) {
            tmpFile.println(line);
            continue;
        }

        StaticJsonDocument<256> doc;
        doc["ts"]     = tok_ts;
        doc["tds"]    = atof(tok_tds);
        doc["turb"]   = atof(tok_turb);
        doc["ph"]     = atof(tok_ph);
        doc["batt_v"] = atof(tok_batt);
        doc["alert"]  = atoi(tok_alrt);
        doc["resync"] = true;

        char payload[256];
        serializeJson(doc, payload);

        bool published = mqtt.publish(TOPIC_TELEMETRY, payload, false);

        if (published) {
            totalSynced++;
            String updatedLine = line.substring(0, line.lastIndexOf(',') + 1) + "1";
            tmpFile.println(updatedLine);
            Serial.printf("syncOfflineData: Successfully sent line %d\n", totalSynced);
        } else {
            tmpFile.println(line);
            Serial.println("syncOfflineData: Failed to send, will retry later.");
        }

        mqtt.loop();
        yield();
    }

    srcFile.close();
    tmpFile.close();

    SD.remove("/log.csv");
    File newSrc  = SD.open("/log_tmp.csv", FILE_READ);
    File newDest = SD.open("/log.csv",     FILE_WRITE);
    if (newSrc && newDest) {
        while (newSrc.available()) {
            newDest.write(newSrc.read());
        }
    }
    if (newSrc)  newSrc.close();
    if (newDest) newDest.close();
    SD.remove("/log_tmp.csv");

    Serial.printf("syncOfflineData complete: %d offline lines, %d successfully synchronized.\n",
                  totalUnsent, totalSynced);
}
