#ifndef INFLUX_MANAGER_H
#define INFLUX_MANAGER_H

#include <Arduino.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

class InfluxManager {
public:
    void begin();
    void loop(); 
    
    void reportSystemMetrics();
    // Aggiunto pressure
    void reportSensorMetrics(float temp, float hum, float pressure, float target);
    void reportRelayState(bool state, const char* source);
    void reportNetworkMetrics();
    
    void reportEvent(const char* category, const char* action, const char* details);
};

extern InfluxManager influx;

#endif