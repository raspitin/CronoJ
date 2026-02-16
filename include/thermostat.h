#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include <Arduino.h>
#include <WiFiUdp.h> 
#include <time.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>

class Thermostat {
public:
    enum class ControlMode : uint8_t {
        AUTO = 0,
        FORCE_OFF,
        FORCE_HEAT,
        MANUAL_TARGET
    };

    Thermostat();
    void setup(); 
    void run();   

    // Setters/Getters
    void update(float currentTemp);
    void setTarget(float target);
    float getTarget();
    
    // Getter Sensori
    float getCurrentTemp();
    float getHumidity();
    float getPressure(); 
    
    bool isHeatingState();

    // Boost & Manuale
    bool startBoost(int minutes);
    bool stopBoost();
    bool isBoostActive();
    long getBoostRemainingSeconds();
    
    // Toggle Override
    void toggleOverride(); 
    bool isOverrideActive();
    void setControlMode(ControlMode mode);
    ControlMode getControlMode() const;
    const char *getControlModeName() const;

    // Manuale diretto (SPOSTATI QUI: DEVONO ESSERE PUBBLICI)
    bool startHeating();
    bool stopHeating();

    // Heartbeat & Stato Relè Remoto
    void checkHeartbeat(bool force = false);
    bool isRelayOnline(); 
    String getRelayIP();
    bool verifyRelayAvailability(bool force = false);

private:
    // Sensori I2C
    Adafruit_AHTX0 aht;
    Adafruit_BMP280 bmp;
    bool sensorsReady = false;
    
    // Caching letture
    unsigned long last_read_time;
    float cached_temp;
    float cached_hum;
    float cached_press;
    void readSensors(); 

    // Stato Termostato
    float currentTemp = 0.0; 
    float currentHumidity = 0.0;
    float targetTemp = 19.0;
    bool isHeating = false;
    
    bool _boostActive = false;
    time_t _boostEndTime = 0;
    ControlMode _controlMode = ControlMode::AUTO;
    float _manualTargetTemp = 22.0f;

    // Gestione Rete UDP
    WiFiUDP _discoveryUdp;
    IPAddress _relayIP;     
    bool _relayOnline = false;
    unsigned long _lastHeartbeat = 0;
    unsigned long _lastRelayStatusCheckMs = 0;
    unsigned long _lastRelayCmdAttemptMs = 0;

    void checkDiscovery();
    void sendDiscovery();
    bool queryRelayStatus(bool *is_on = nullptr);
    bool sendRelayCommand(const char *endpoint, bool expected_on);
    float getScheduledTarget();
};

#endif
