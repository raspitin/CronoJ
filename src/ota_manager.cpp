#include "ota_manager.h"

#include <ArduinoOTA.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "ui.h"

static bool ota_started = false;
static volatile bool ota_in_progress = false;
static uint32_t ota_last_print_ms = 0;
static uint32_t ota_last_ui_ms = 0;
static uint8_t ota_last_ui_pct = 255;

void ota_setup() {
    if (WiFi.status() != WL_CONNECTED) {
        ota_started = false;
        ota_in_progress = false;
        return;
    }
    if (ota_started) return;

    ArduinoOTA.setHostname(DEVICE_HOSTNAME);
    ArduinoOTA.setPort(OTA_ARDUINO_PORT);
    if (OTA_ARDUINO_PASSWORD[0] != '\0') {
        ArduinoOTA.setPassword(OTA_ARDUINO_PASSWORD);
    }

    ArduinoOTA.onStart([]() {
        esp_task_wdt_reset();
        ota_in_progress = true;
        ota_last_print_ms = 0;
        ota_last_ui_ms = 0;
        ota_last_ui_pct = 255;
        ui_show_ota_screen(0);
        Serial.println("OTA PlatformIO: avvio aggiornamento");
    });
    ArduinoOTA.onEnd([]() {
        esp_task_wdt_reset();
        ui_show_ota_screen(100);
        ota_in_progress = false;
        Serial.println("OTA PlatformIO: completato");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        esp_task_wdt_reset();
        const uint32_t now = millis();
        const uint32_t pct = (total > 0) ? ((progress * 100U) / total) : 0U;
        const uint8_t pct8 = (uint8_t)pct;

        const bool pct_changed = (pct8 != ota_last_ui_pct);
        const bool force_ui = (pct8 == 0U) || (progress == total) || ((now - ota_last_ui_ms) >= 200U);
        if (pct_changed && force_ui) {
            ui_show_ota_screen(pct8);
            ota_last_ui_pct = pct8;
            ota_last_ui_ms = now;
        }
        if ((now - ota_last_print_ms) >= 500U || progress == total) {
            Serial.printf("OTA PlatformIO: %u%%\n", (unsigned)pct);
            ota_last_print_ms = now;
        }
    });
    ArduinoOTA.onError([](ota_error_t error) {
        esp_task_wdt_reset();
        ota_in_progress = false;
        ui_hide_ota_screen();
        Serial.printf("OTA PlatformIO errore: %u\n", (unsigned)error);
    });

    ArduinoOTA.setRebootOnSuccess(true);
    ArduinoOTA.begin();
    ota_started = true;
    Serial.printf("OTA PlatformIO pronto: %s.local:%u\n", DEVICE_HOSTNAME, OTA_ARDUINO_PORT);
}

void ota_handle() {
    if (WiFi.status() != WL_CONNECTED) {
        ota_started = false;
        return;
    }
    if (!ota_started) {
        ota_setup();
    }
    ArduinoOTA.handle();
}

bool ota_is_in_progress() {
    return ota_in_progress;
}
