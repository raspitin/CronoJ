#include "influx_manager.h"
#include "secrets.h"
#include "config.h"
#include "config_manager.h" 
#include <WiFi.h>

// Puntatore globale
InfluxDBClient* influxClient = nullptr;
static bool influxConnected = false;
static unsigned long nextInfluxRetryAtMs = 0;
static const unsigned long INFLUX_RETRY_INTERVAL_MS = 30000;
static bool influxOfflineLogged = false;

InfluxManager influx;

static bool influxCanRetryNow() {
    return millis() >= nextInfluxRetryAtMs;
}

static void influxScheduleRetry() {
    influxConnected = false;
    nextInfluxRetryAtMs = millis() + INFLUX_RETRY_INTERVAL_MS;
}

static bool influxTryConnect(bool forceLog) {
    if (influxClient == nullptr) return false;
    if (!WiFi.isConnected()) {
        influxConnected = false;
        return false;
    }
    if (!influxConnected && !forceLog && !influxCanRetryNow()) {
        return false;
    }

    if (influxClient->validateConnection()) {
        if (!influxConnected || forceLog) {
            Serial.print("Connesso a InfluxDB: ");
            Serial.println(influxClient->getServerUrl());
        }
        influxConnected = true;
        influxOfflineLogged = false;
        nextInfluxRetryAtMs = 0;
        return true;
    }

    influxScheduleRetry();
    if (forceLog || !influxOfflineLogged) {
        Serial.print("Errore InfluxDB: ");
        Serial.println(influxClient->getLastErrorMessage());
        influxOfflineLogged = true;
    }
    return false;
}

void writePointDebug(Point& p) {
    if (influxClient == nullptr) return;
    if (!influxConnected && !influxTryConnect(false)) return;
    if (!influxClient->writePoint(p)) {
        Serial.print("InfluxDB Write Failed: ");
        Serial.println(influxClient->getLastErrorMessage());
        influxScheduleRetry();
    }
}

void InfluxManager::begin() {
    if (influxClient != nullptr) {
        delete influxClient;
        influxClient = nullptr;
    }

    influxClient = new InfluxDBClient(
        configManager.data.influxUrl,
        configManager.data.influxOrg,
        configManager.data.influxBucket,
        configManager.data.influxToken
    );

    // FIX RIAVVIO: Timeout a 2 secondi per evitare il Watchdog
    influxClient->setHTTPOptions(HTTPOptions().httpReadTimeout(2000).connectionReuse(true));
    influxClient->setInsecure(); 

    influxConnected = false;
    nextInfluxRetryAtMs = 0;
    influxOfflineLogged = false;
    influxTryConnect(true);
}

void InfluxManager::loop() {
    if (!influxConnected) {
        influxTryConnect(false);
    }
}

void InfluxManager::reportSystemMetrics() {
    if(!WiFi.isConnected()) return;
    Point p("system");
    p.addTag("device", DEVICE_NAME);
    p.addField("heap_free", (long)ESP.getFreeHeap());
    p.addField("uptime_sec", (long)(millis() / 1000));
    p.addField("wifi_rssi", (long)WiFi.RSSI());
    writePointDebug(p);
}

// FIRMA CORRETTA: 4 ARGOMENTI (PRESSURE INCLUSA)
void InfluxManager::reportSensorMetrics(float temp, float hum, float pressure, float target) {
    if(!WiFi.isConnected()) return;

    Point p("sensors");
    p.addTag("device", DEVICE_NAME);

    if (!isnan(temp)) p.addField("temperature", temp);
    if (!isnan(hum)) p.addField("humidity", hum);
    // Aggiunta Pressione
    if (!isnan(pressure) && pressure > 0) p.addField("pressure", pressure);
    
    p.addField("target_temp", target);
    
    if (!isnan(temp) && !isnan(hum)) {
        double a = 17.27; double b = 237.7;
        double alpha = ((a * temp) / (b + temp)) + log(hum/100.0);
        double dew_point = (b * alpha) / (a - alpha);
        p.addField("dew_point", dew_point);
    }

    float heat_need = target - temp;
    if (isnan(heat_need) || heat_need < 0) heat_need = 0;
    p.addField("heating_need", heat_need);

    writePointDebug(p);
}

void InfluxManager::reportRelayState(bool state, const char* source) {
    if(!WiFi.isConnected()) return;
    Point p("relay");
    p.addTag("device", DEVICE_NAME);
    p.addTag("source", source); 
    p.addField("state", state ? 1 : 0);
    writePointDebug(p);
}

void InfluxManager::reportNetworkMetrics() {
    if(!WiFi.isConnected()) return;
    Point p("network");
    p.addTag("device", DEVICE_NAME);
    p.addField("ip", WiFi.localIP().toString());
    p.addField("rssi", WiFi.RSSI());
    writePointDebug(p);
}

void InfluxManager::reportEvent(const char* category, const char* action, const char* details) {
    if(!WiFi.isConnected()) return;
    Point p("events");
    p.addTag("device", DEVICE_NAME);
    p.addTag("category", category);
    p.addTag("action", action);
    p.addField("details", details);
    writePointDebug(p);
}
