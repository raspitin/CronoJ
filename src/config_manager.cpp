#include "config_manager.h"

ConfigManager configManager;

bool ConfigManager::begin() {
    if (!LittleFS.begin(false)) { 
        Serial.println("LittleFS Mount Failed");
        return false;
    }
    return load();
}

bool ConfigManager::load() {
    if (!LittleFS.exists(filename)) {
        Serial.println("Config file not found, creating default.");
        return saveConfig();
    }

    File file = LittleFS.open(filename, "r");
    if (!file) {
        Serial.println("Failed to open config file");
        return false;
    }

    DynamicJsonDocument doc(2048); // Aumentato buffer per contenere tutto
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print("Failed to parse config file: ");
        Serial.println(error.c_str());
        return false;
    }
    
    Serial.println("Config file loaded successfully");

    // --- Caricamento Schedule ---
    JsonArray sched = doc["schedule"];
    if (!sched.isNull() && sched.size() == 7) {
        for(int i=0; i<7; i++) {
            data.weekSchedule[i].timeSlots = sched[i];
        }
    }

    // --- Caricamento Altri Dati ---
    strlcpy(data.weatherKey, doc["weatherKey"] | "", sizeof(data.weatherKey));
    strlcpy(data.weatherApiUrl, doc["weatherApiUrl"] | "http://api.openweathermap.org/data/2.5/forecast", sizeof(data.weatherApiUrl));
    strlcpy(data.weatherCity, doc["weatherCity"] | "Roma", sizeof(data.weatherCity));
    strlcpy(data.weatherCountry, doc["weatherCountry"] | "IT", sizeof(data.weatherCountry));
    strlcpy(data.timezone, doc["timezone"] | "CET-1CEST,M3.5.0,M10.5.0/3", sizeof(data.timezone));
    
    strlcpy(data.influxUrl, doc["influxUrl"] | "http://192.168.1.140:8087", sizeof(data.influxUrl));
    strlcpy(data.influxOrg, doc["influxOrg"] | "home_automation", sizeof(data.influxOrg));
    strlcpy(data.influxBucket, doc["influxBucket"] | "termostato_data", sizeof(data.influxBucket));
    strlcpy(data.influxToken, doc["influxToken"] | "", sizeof(data.influxToken));

    data.tempOffset = doc["tempOffset"] | 0.0;

    // Debug: stampa i valori caricati per InfluxDB
    Serial.print("Config loaded - InfluxURL: '");
    Serial.print(data.influxUrl);
    Serial.print("', Token len: ");
    Serial.println(strlen(data.influxToken));

    return true;
}

bool ConfigManager::saveConfig() {
    DynamicJsonDocument doc(2048);

    // --- Salvataggio Schedule ---
    JsonArray sched = doc.createNestedArray("schedule");
    for(int i=0; i<7; i++) {
        sched.add(data.weekSchedule[i].timeSlots);
    }

    doc["weatherKey"] = data.weatherKey;
    doc["weatherApiUrl"] = data.weatherApiUrl;
    doc["weatherCity"] = data.weatherCity;
    doc["weatherCountry"] = data.weatherCountry;
    doc["timezone"] = data.timezone;
    
    doc["influxUrl"] = data.influxUrl;
    doc["influxOrg"] = data.influxOrg;
    doc["influxBucket"] = data.influxBucket;
    doc["influxToken"] = data.influxToken;
    
    doc["tempOffset"] = data.tempOffset;

    File file = LittleFS.open(filename, "w");
    if (!file) return false;

    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to write config file");
        file.close();
        return false;
    }
    file.close();
    return true;
}

void ConfigManager::resetToFactory() {
    data = ConfigData(); 
    saveConfig();
}
