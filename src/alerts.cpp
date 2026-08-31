#include "alerts.h"
#include <LiquidCrystal_I2C.h>

// I2C address 0x27 adalah yang paling umum. Jika LCD tidak menyala,
// coba 0x3F. Bisa juga scan I2C dengan sketch i2c_scanner.ino
LiquidCrystal_I2C lcd(0x27, 16, 2);

void initAlerts() {
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);

    // Matikan semua output pada awalnya
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_GREEN, LOW);

    // Inisialisasi LCD
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Water Monitor");
    lcd.setCursor(0, 1);
    lcd.print("Memulai...");
    delay(1500);
    lcd.clear();
    
    Serial.println("Alerts initialized.");
}

bool checkThresholds(SensorData data) {
    // Jika sensor error, anggap alert agar teknisi datang memeriksa
    if (data.isError) return true;

    return (data.tds       > THRESHOLD_TDS_HIGH        ||
            data.turbidity > THRESHOLD_TURBIDITY_HIGH  ||
            data.ph        < THRESHOLD_PH_LOW          ||
            data.ph        > THRESHOLD_PH_HIGH);
}

void setAlertState(bool isAlert) {
    if (isAlert) {
        digitalWrite(PIN_LED_RED,   HIGH);
        digitalWrite(PIN_LED_GREEN, LOW);
        // Buzzer: beep pendek 3x (non-blocking via tone)
        for (int i = 0; i < 3; i++) {
            digitalWrite(PIN_BUZZER, HIGH);
            delay(150);
            digitalWrite(PIN_BUZZER, LOW);
            delay(100);
        }
    } else {
        digitalWrite(PIN_LED_RED,   LOW);
        digitalWrite(PIN_LED_GREEN, HIGH);
        digitalWrite(PIN_BUZZER,    LOW);
    }
}

void updateDisplay(SensorData data, bool isAlert) {
    lcd.clear();

    if (data.isError) {
        lcd.setCursor(0, 0);
        lcd.print("!SENSOR ERROR!");
        lcd.setCursor(0, 1);
        lcd.print("Cek koneksi");
        return;
    }

    // Baris 1: pH dan TDS
    lcd.setCursor(0, 0);
    lcd.print("pH:");
    lcd.print(data.ph, 1);
    lcd.print(" TDS:");
    // TDS: cetak integer, truncate jika > 999 agar tidak overflow layar 16 char
    if (data.tds >= 1000.0f) {
        lcd.print(">999");
    } else {
        lcd.print((int)data.tds);
    }

    // Baris 2: Turbidity dan status
    lcd.setCursor(0, 1);
    lcd.print("Tb:");
    if (data.turbidity >= 1000.0f) {
        lcd.print(">999");
    } else {
        lcd.print((int)data.turbidity);
    }
    lcd.print("NTU ");
    lcd.print(isAlert ? "!AWAS!" : "AMAN");
}

void silenceAlerts() {
    // Matikan semua output sebelum deep sleep agar tidak buang daya
    digitalWrite(PIN_BUZZER,     LOW);
    digitalWrite(PIN_LED_RED,    LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_GREEN,  LOW);
    lcd.noBacklight();
    lcd.noDisplay();
}
