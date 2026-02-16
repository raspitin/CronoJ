#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// Struttura per la programmazione giornaliera (ripristinata)
struct DaySchedule {
    uint64_t timeSlots; // Bitmask per 48 slot da 30 min (0=Eco, 1=Comfort)
};

struct ConfigData {
    // --- Programmazione (RIPRISTINATA) ---
    DaySchedule weekSchedule[7]; 

    // --- Meteo & Localizzazione ---
    char weatherKey[64];
    char weatherApiUrl[128];
    char weatherCity[32];
    char weatherCountry[4];
    char timezone[64];

    // --- InfluxDB (Nuovi campi) ---
    char influxUrl[64];
    char influxOrg[32];
    char influxBucket[32];
    char influxToken[128]; 

    // --- Calibrazione ---
    float tempOffset;
    
    // --- Default Constructor ---
    ConfigData() {
        // Default Schedule: tutto 0 (Eco/Off)
        for(int i=0; i<7; i++) weekSchedule[i].timeSlots = 0;

        strcpy(weatherKey, "");
        strcpy(weatherApiUrl, "http://api.openweathermap.org/data/2.5/forecast");
        strcpy(weatherCity, "Roma");
        strcpy(weatherCountry, "IT");
        strcpy(timezone, "CET-1CEST,M3.5.0,M10.5.0/3");
        
        // Default InfluxDB
        strcpy(influxUrl, "http://192.168.1.140:8087");
        strcpy(influxOrg, "home_automation");
        strcpy(influxBucket, "termostato_data");
        strcpy(influxToken, ""); 
        
        tempOffset = 0.0;
    }
};

class ConfigManager {
public:
    ConfigData data;
    bool begin();
    bool saveConfig(); // Rinominato per compatibilità con ui.cpp
    bool load();
    void resetToFactory();

private:
    const char* filename = "/config.json";
};

extern ConfigManager configManager;

#endif
