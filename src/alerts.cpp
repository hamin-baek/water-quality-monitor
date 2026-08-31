#include "alerts.h"
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void initAlerts() {
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);

    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_GREEN, LOW);

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Water Monitor");
    lcd.setCursor(0, 1);
    lcd.print("Starting...");
    delay(1500);
    lcd.clear();
    
    Serial.println("Alerts initialized.");
}

bool checkThresholds(SensorData data) {
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
        lcd.print("Check connection");
        return;
    }

    lcd.setCursor(0, 0);
    lcd.print("pH:");
    lcd.print(data.ph, 1);
    lcd.print(" TDS:");
    if (data.tds >= 1000.0f) {
        lcd.print(">999");
    } else {
        lcd.print((int)data.tds);
    }

    lcd.setCursor(0, 1);
    lcd.print("Tb:");
    if (data.turbidity >= 1000.0f) {
        lcd.print(">999");
    } else {
        lcd.print((int)data.turbidity);
    }
    lcd.print("NTU ");
    lcd.print(isAlert ? "!WARN!" : "SAFE");
}

void silenceAlerts() {
    digitalWrite(PIN_BUZZER,     LOW);
    digitalWrite(PIN_LED_RED,    LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_GREEN,  LOW);
    lcd.noBacklight();
    lcd.noDisplay();
}
