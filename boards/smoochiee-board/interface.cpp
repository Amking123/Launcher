#include "powerSave.h"
#include <Wire.h>
#include <XPowersLib.h>
#include <I2CKeyPad.h>

XPowersPPM PPM;

// I2C keypad
#define KEYPAD_ADDR 0x20
I2CKeyPad keypad(KEYPAD_ADDR);

void _setup_gpio() {
    // --------------------------
    // Remove GPIO pinMode setup for buttons
    // --------------------------
    // Keep SPI pins
    pinMode(CC1101_SS_PIN, OUTPUT);
    pinMode(NRF24_SS_PIN, OUTPUT);
    pinMode(45, OUTPUT);

    digitalWrite(45, HIGH);
    digitalWrite(CC1101_SS_PIN, HIGH);
    digitalWrite(NRF24_SS_PIN, HIGH);

    // Power management
    bool pmu_ret = false;
    Wire.begin(GROVE_SDA, GROVE_SCL);
    pmu_ret = PPM.init(Wire, GROVE_SDA, GROVE_SCL, BQ25896_SLAVE_ADDRESS);
    if (pmu_ret) {
        PPM.setSysPowerDownVoltage(3300);
        PPM.setInputCurrentLimit(3250);
        Serial.printf("getInputCurrentLimit: %d mA\n", PPM.getInputCurrentLimit());
        PPM.disableCurrentLimitPin();
        PPM.setChargeTargetVoltage(4208);
        PPM.setPrechargeCurr(64);
        PPM.setChargerConstantCurr(832);
        PPM.getChargerConstantCurr();
        Serial.printf("getChargerConstantCurr: %d mA\n", PPM.getChargerConstantCurr());
        PPM.enableADCMeasure();
        PPM.enableCharge();
        PPM.enableOTG();
        PPM.disableOTG();
    }

    // Initialize I2C keypad
    keypad.begin(Wire);
}

// ---------------- BATTERY ----------------
int getBattery() {
    uint8_t percent = 0;
    percent = (PPM.getSystemVoltage() - 3300) * 100 / (float)(4150 - 3350);
    return (percent < 0) ? 0 : (percent >= 100) ? 100 : percent;
}

// ---------------- BRIGHTNESS ----------------
void _setBrightness(uint8_t brightval) {
    int bl = MINBRIGHT + round(((255 - MINBRIGHT) * bright / 100));
    analogWrite(TFT_BL, bl);
}

// ---------------- INPUT HANDLER ----------------
void InputHandler(void) {
    static unsigned long tm = 0;
    if (millis() - tm < 200 && !LongPress) return;

    // read key from keypad
    uint8_t key = keypad.getKey();
    if (key == I2C_KEYPAD_NOKEY) return;

    if (!wakeUpScreen()) AnyKeyPress = true;
    else return;

    // Map keypad codes to original button names
    bool l = (key == 5);    // L_BTN
    bool r = (key == 6);    // R_BTN
    bool u = (key == 1);    // UP_BTN
    bool d = (key == 13);   // DW_BTN
    bool s = (key == 4);    // SEL_BTN

    if (l) PrevPress = true;
    if (r) NextPress = true;
    if (u) UpPress = true;
    if (d) DownPress = true;
    if (s) SelPress = true;

    // EscPress remains Left+Right
    if (PrevPress && NextPress) {
        PrevPress = false;
        NextPress = false;
        EscPress = true;
    }

    tm = millis();
}

// ---------------- POWER OFF ----------------
void powerOff() {
    // Wake on SELECT no longer possible via I2C
    esp_deep_sleep_start();
}

// ---------------- REBOOT / LONG PRESS ----------------
void checkReboot() {
    int countDown;
    static unsigned long time_count = 0;
    static bool counting = false;

    uint8_t key = keypad.getKey();
    bool leftPressed  = (key == 5);
    bool rightPressed = (key == 6);

    if (leftPressed && rightPressed) {
        if (!counting) {
            time_count = millis();
            counting = true;
        }

        while (leftPressed && rightPressed) {
            if (millis() - time_count > 500) {
                tft->setTextSize(1);
                tft->setTextColor(FGCOLOR, BGCOLOR);
                countDown = (millis() - time_count) / 1000 + 1;
                if (countDown < 4)
                    tft->drawCentreString("PWR OFF IN " + String(countDown) + "/3", tftWidth / 2, 12, 1);
                else {
                    tft->fillScreen(BGCOLOR);
                    while (keypad.getKey() == 5 || keypad.getKey() == 6); // wait release
                    delay(200);
                    powerOff();
                }
                delay(10);
            }
            key = keypad.getKey();
            leftPressed  = (key == 5);
            rightPressed = (key == 6);
        }

        // Clear text after releasing the button
        delay(30);
        tft->fillRect(60, 12, tftWidth - 60, 8, BGCOLOR);
        counting = false;
    }
}
