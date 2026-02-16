#include <Arduino.h>
#include <esp32_smartdisplay.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include "ui.h"
#include "network_manager.h"
#include "config_manager.h"
#include "web_interface.h"
#include "thermostat.h"
#include <time.h>      
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "secrets.h"
#include "config.h"
#include "mqtt_manager.h"
#include "influx_manager.h" 
#include "ota_manager.h"

// FIX RIAVVIO: Watchdog a 30 secondi
#define WDT_TIMEOUT 30 

Thermostat thermo;
bool isOnline = false;
unsigned long last_tick_millis = 0;
unsigned long last_wifi_check = 0; 
bool time_synced = false; 

unsigned long last_influx_system = 0;
unsigned long last_influx_sensors = 0;

WiFiUDP udpClient;
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_NAME, "app", LOG_KERN);

typedef struct
{
    char now_temp[16];
    char now_desc[64];
    char now_icon[8];
    bool has_tomorrow;
    char tmr_temp[16];
    char tmr_desc[64];
    char tmr_icon[8];
} WeatherUiUpdate;

static QueueHandle_t weather_ui_queue = nullptr;
static TaskHandle_t weather_task_handle = nullptr;

static bool is_time_valid() {
    const time_t now = time(nullptr);
    // 2024-01-01 00:00:00 UTC
    return now >= 1704067200;
}

static bool wait_for_ntp_sync(uint32_t timeout_ms) {
    const unsigned long start = millis();
    while ((millis() - start) < timeout_ms) {
        if (is_time_valid()) return true;
        delay(250);
        esp_task_wdt_reset();
    }
    return false;
}

void logMsg(String msg, uint16_t level = LOG_INFO) {
    Serial.println(msg);
    if (isOnline && WiFi.status() == WL_CONNECTED) {
        syslog.log(level, msg);
    }
}

void fetch_weather() {
    if (WiFi.status() != WL_CONNECTED) return;
    esp_task_wdt_reset();
    logMsg("--- Scaricamento Meteo ---");
    
    String city = String(configManager.data.weatherCity); city.trim();
    String country = String(configManager.data.weatherCountry); country.trim();
    String key = String(configManager.data.weatherKey); key.trim();
    String apiBaseUrl = String(configManager.data.weatherApiUrl); apiBaseUrl.trim();
    if (apiBaseUrl.length() == 0) {
        apiBaseUrl = "http://api.openweathermap.org/data/2.5/forecast";
    }

    HTTPClient http;
    const String sep = apiBaseUrl.indexOf('?') >= 0 ? "&" : "?";
    String url = apiBaseUrl + sep + "q=" + city + "," + country + "&appid=" + key + "&units=metric&lang=it&cnt=10";
    logMsg("Meteo request: url=" + apiBaseUrl + ", citta=" + city + ", paese=" + country + ", timeout=5000ms");

    http.setTimeout(5000); 
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode < 0) {
        logMsg("Meteo HTTP code: " + String(httpCode) + " (" + http.errorToString(httpCode) + ")", LOG_ERR);
    } else {
        logMsg("Meteo HTTP code: " + String(httpCode));
    }

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        StaticJsonDocument<256> filter;
        filter["list"][0]["main"]["temp"] = true;
        filter["list"][0]["weather"][0]["description"] = true;
        filter["list"][0]["weather"][0]["icon"] = true;
        filter["list"][8]["main"]["temp"] = true;
        filter["list"][8]["weather"][0]["description"] = true;
        filter["list"][8]["weather"][0]["icon"] = true;

        DynamicJsonDocument doc(10240); 
        DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

        if (!error) {
            JsonObject itemNow = doc["list"][0];
            float tempNow = itemNow["main"]["temp"];
            const char* descNow = itemNow["weather"][0]["description"];
            const char* iconNow = itemNow["weather"][0]["icon"];
            String dNow = String(descNow); 
            if(dNow.length() > 0) dNow[0] = toupper(dNow[0]);
            WeatherUiUpdate ui_update = {};
            String temp_now_str = String(tempNow, 1);
            snprintf(ui_update.now_temp, sizeof(ui_update.now_temp), "%s", temp_now_str.c_str());
            snprintf(ui_update.now_desc, sizeof(ui_update.now_desc), "%s", dNow.c_str());
            snprintf(ui_update.now_icon, sizeof(ui_update.now_icon), "%s", String(iconNow).c_str());
            ui_update.has_tomorrow = false;

            if (doc["list"].size() > 8) {
                JsonObject itemTmrw = doc["list"][8];
                float tempTmrw = itemTmrw["main"]["temp"];
                const char* descTmrw = itemTmrw["weather"][0]["description"];
                const char* iconTmrw = itemTmrw["weather"][0]["icon"];
                String dTmrw = String(descTmrw); 
                if(dTmrw.length() > 0) dTmrw[0] = toupper(dTmrw[0]);
                String temp_tmr_str = String(tempTmrw, 1);
                snprintf(ui_update.tmr_temp, sizeof(ui_update.tmr_temp), "%s", temp_tmr_str.c_str());
                snprintf(ui_update.tmr_desc, sizeof(ui_update.tmr_desc), "%s", dTmrw.c_str());
                snprintf(ui_update.tmr_icon, sizeof(ui_update.tmr_icon), "%s", String(iconTmrw).c_str());
                ui_update.has_tomorrow = true;
            }

            if (weather_ui_queue != nullptr) {
                xQueueOverwrite(weather_ui_queue, &ui_update);
            }
            logMsg("Meteo OK.");
        } else {
            logMsg("JSON Error: " + String(error.c_str()), LOG_ERR);
        }
    } else {
        logMsg("Meteo KO: " + http.errorToString(httpCode), LOG_ERR);
    }
    http.end();
}

static void weather_task(void *param) {
    (void)param;
    const TickType_t interval_ok = pdMS_TO_TICKS(1800000); // 30 min
    const TickType_t interval_retry = pdMS_TO_TICKS(10000); // 10 s

    vTaskDelay(pdMS_TO_TICKS(1500));
    for (;;) {
        if (isOnline && WiFi.status() == WL_CONNECTED) {
            fetch_weather();
            vTaskDelay(interval_ok);
        } else {
            vTaskDelay(interval_retry);
        }
    }
}

void setup() {
    Serial.begin(115200);
    smartdisplay_init();
    ui_show_splash();
    lv_timer_handler(); 
    delay(100);

    if(!configManager.begin()) Serial.println("FS Error");
    ui_init_all(); 
    weather_ui_queue = xQueueCreate(1, sizeof(WeatherUiUpdate));
    
    last_tick_millis = millis();
    isOnline = setup_network();
    if (!isOnline) {
        ui_splash_config_mode();
    }
    
    if (isOnline) {
        syslog.server(SYSLOG_SERVER, SYSLOG_PORT);
        logMsg("Avvio Sistema. Heap: " + String(ESP.getFreeHeap()));
        mqtt_setup(); 
        ota_setup();
        
        configTime(0, 0, "it.pool.ntp.org", "time.nist.gov", "pool.ntp.org");
        setenv("TZ", configManager.data.timezone, 1);
        tzset();
        if (wait_for_ntp_sync(15000)) {
            time_synced = true;
            time_t now_ts = time(nullptr);
            struct tm timeinfo;
            if (localtime_r(&now_ts, &timeinfo)) {
                logMsg("NTP OK: " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min));
            }
        } else {
            logMsg("NTP non sincronizzato al boot: continuo e ritento in loop.", LOG_WARNING);
        }

        influx.begin();
        influx.reportEvent("system", "boot", "Device restarted");

        thermo.setup();

        logMsg("Verifica Relè...");
        thermo.checkHeartbeat(true); 
        unsigned long relay_wait_start = millis();
        while (!thermo.isRelayOnline() && (millis() - relay_wait_start < 1500)) {
            thermo.checkHeartbeat(false);
            esp_task_wdt_reset();
            delay(100);
        }
        if (thermo.verifyRelayAvailability(true)) {
            logMsg("Relè raggiungibile e verificato: " + thermo.getRelayIP());
        } else {
            logMsg("Relè non raggiungibile/verificato al boot (controlla rete e servizio relè).", LOG_WARNING);
        }
        setup_web_server();
    }

    if (weather_task_handle == nullptr) {
        xTaskCreatePinnedToCore(
            weather_task,
            "weather_task",
            8192,
            nullptr,
            1,
            &weather_task_handle,
            0
        );
    }

    lv_scr_load(scr_main);
    Serial.println("Watchdog Start...");
    esp_task_wdt_init(WDT_TIMEOUT, true); 
    esp_task_wdt_add(NULL); 
}

void loop() {
    esp_task_wdt_reset();
    ota_handle();

    if (ota_is_in_progress()) {
        const unsigned long current_millis = millis();
        lv_tick_inc(current_millis - last_tick_millis);
        last_tick_millis = current_millis;
        lv_timer_handler();
        delay(16);
        return;
    }

    if (isOnline) {
        mqtt_loop(); 
        influx.loop();
        unsigned long now = millis();

        if (now - last_influx_system > 60000) {
            influx.reportSystemMetrics();
            last_influx_system = now;
        }

        if (now - last_influx_sensors > 30000) {
            // PASSAGGIO CORRETTO: 4 ARGOMENTI (PRESSURE INCLUSA)
            influx.reportSensorMetrics(
                thermo.getCurrentTemp(), 
                thermo.getHumidity(), 
                thermo.getPressure(), 
                thermo.getTarget()
            );
            last_influx_sensors = now;
        }
    }

    static unsigned long last_mqtt_pub = 0;
    if (millis() - last_mqtt_pub > 5000) { 
        mqtt_publish_state(thermo.getCurrentTemp(), thermo.getTarget(), thermo.isHeatingState());
        last_mqtt_pub = millis();
    }

    thermo.run();
    static int lastRelayOnlineState = -1;
    int relayOnlineState = thermo.isRelayOnline() ? 1 : 0;
    if (relayOnlineState != lastRelayOnlineState) {
        if (relayOnlineState == 1) {
            logMsg("Relè online: " + thermo.getRelayIP());
        } else {
            logMsg("Relè offline: heartbeat non ricevuto", LOG_WARNING);
        }
        lastRelayOnlineState = relayOnlineState;
    }
    web_interface_loop();

    if (weather_ui_queue != nullptr) {
        WeatherUiUpdate ui_update;
        if (xQueueReceive(weather_ui_queue, &ui_update, 0) == pdTRUE) {
            update_current_weather(
                String(ui_update.now_temp),
                String(ui_update.now_desc),
                String(ui_update.now_icon)
            );
            if (ui_update.has_tomorrow) {
                update_forecast_item(
                    1,
                    "Domani",
                    String(ui_update.tmr_temp),
                    String(ui_update.tmr_desc),
                    String(ui_update.tmr_icon)
                );
            }
        }
    }

    static bool lastRelayState = false; 
    bool currentRelayState = thermo.isHeatingState();
    if (currentRelayState != lastRelayState) {
        influx.reportRelayState(currentRelayState, "thermostat_logic");
        lastRelayState = currentRelayState;
    }

    if (millis() - last_wifi_check > 60000) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi perso! Tento riconnessione...");
            network_stop_mdns();
            WiFi.reconnect();
            isOnline = false;
        } else {
            if (!isOnline) {
                Serial.println("WiFi Ripristinato!");
                isOnline = true;
                network_start_mdns();
                ota_setup();
                if(!time_synced) configTime(0, 0, "it.pool.ntp.org", "time.nist.gov", "pool.ntp.org");
                thermo.checkHeartbeat(true); 
                influx.begin(); 
            }
        }
        last_wifi_check = millis();
    }

    unsigned long current_millis = millis();
    lv_tick_inc(current_millis - last_tick_millis);
    last_tick_millis = current_millis;
    lv_timer_handler();
    
    static unsigned long last_ui_update = 0;
    if (current_millis - last_ui_update > 1000) {
        update_ui(); 
        if (!time_synced) {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 0)) {
                time_synced = true;
                logMsg("NTP OK: " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min));
            }
        }
        last_ui_update = current_millis;
    }

    delay(5);
}
