#include "thermostat.h"
#include "config.h"
#include "config_manager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "secrets.h"
#include <time.h>

#define SENSOR_READ_INTERVAL 2000 
#define RELAY_TIMEOUT_MS 15000 
#define HEARTBEAT_INTERVAL (60 * 1000)
#define RELAY_STATUS_CHECK_INTERVAL_MS 5000
#define RELAY_CMD_MIN_GAP_MS 500

// Pin I2C per i SENSORI (Usiamo Wire1 per non confliggere con il Touch su Wire0)
#define I2C_SDA_PIN 17  
#define I2C_SCL_PIN 18

// --- MODIFICA CRITICA NEL COSTRUTTORE ---
// Inizializziamo 'bmp' passando il puntatore a &Wire1 nella lista di inizializzazione
Thermostat::Thermostat() : bmp(&Wire1) {
    #ifdef RELAY_PIN
    if (RELAY_PIN >= 0) {
        pinMode(RELAY_PIN, OUTPUT);
        digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
    }
    #endif
    
    _relayIP.fromString(DEFAULT_REMOTE_RELAY_IP);

    last_read_time = 0;
    cached_temp = 0.0;
    cached_hum = 0.0;
    cached_press = 0.0;
}

void Thermostat::setup() {
    _discoveryUdp.begin(DISCOVERY_PORT);

    // --- INIZIALIZZAZIONE I2C SUI SENSORI (WIRE1) ---
    // Usiamo Wire1 per evitare il conflitto con il Touch (che usa Wire su 19/20).
    // setPins evita di fare begin() qui: il primo begin lo fa la libreria sensori.
    Wire1.setPins(I2C_SDA_PIN, I2C_SCL_PIN);
    delay(50); 

    // Passiamo &Wire1 anche all'AHT20
    bool aht_ok = aht.begin(&Wire1);
    
    // Il BMP280 è già stato legato a &Wire1 nel costruttore
    uint8_t bmp_addr = 0;
    Wire1.beginTransmission(0x76);
    if (Wire1.endTransmission() == 0) {
        bmp_addr = 0x76;
    } else {
        Wire1.beginTransmission(0x77);
        if (Wire1.endTransmission() == 0) {
            bmp_addr = 0x77;
        }
    }
    bool bmp_ok = (bmp_addr != 0) ? bmp.begin(bmp_addr) : false;
    Wire1.setClock(100000);

    if (aht_ok && bmp_ok) {
        Serial.println("Sensori AHT20 e BMP280 OK su Wire1!");
        sensorsReady = true;
    } else {
        Serial.println("ERRORE SENSORI I2C (Wire1)!");
        if(!aht_ok) Serial.println("- AHT20 mancante");
        if(!bmp_ok) Serial.println("- BMP280 mancante");
        sensorsReady = false;
    }
    
    readSensors();
}

void Thermostat::readSensors() {
    if (!sensorsReady) return;
    
    unsigned long now = millis();
    if (now - last_read_time < SENSOR_READ_INTERVAL && last_read_time > 0) return;

    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp); 

    cached_temp = temp.temperature;
    cached_hum = humidity.relative_humidity;
    cached_press = bmp.readPressure() / 100.0F; 

    // Aggiorna variabili di stato
    this->currentTemp = cached_temp + configManager.data.tempOffset; 
    this->currentHumidity = cached_hum;

    last_read_time = now;
}

float Thermostat::getCurrentTemp() {
    readSensors();
    if (isnan(this->currentTemp)) return 0.0;
    return this->currentTemp;
}

float Thermostat::getHumidity() {
    readSensors();
    if (isnan(this->currentHumidity)) return 0.0;
    return this->currentHumidity;
}

float Thermostat::getPressure() {
    readSensors();
    if (isnan(cached_press)) return 0.0;
    return cached_press;
}

bool Thermostat::isHeatingState() { 
    return isHeating; 
}

// ... Logica Discovery ...
void Thermostat::checkDiscovery() {
    int packetSize = _discoveryUdp.parsePacket();
    if (packetSize) {
        char packetBuffer[255];
        int len = _discoveryUdp.read(packetBuffer, 255);
        if (len > 0) packetBuffer[len] = 0;
        
        String msg = String(packetBuffer);
        if (msg.startsWith("RELAY_HERE_V1")) {
            _relayIP = _discoveryUdp.remoteIP();
            _relayOnline = true;
            _lastHeartbeat = millis();
        }
    }
}

void Thermostat::sendDiscovery() {
    _discoveryUdp.beginPacket(IPAddress(255,255,255,255), DISCOVERY_PORT);
    _discoveryUdp.print(DISCOVERY_PACKET_CONTENT);
    _discoveryUdp.endPacket();
}

void Thermostat::checkHeartbeat(bool force) {
    if (force || millis() - _lastHeartbeat > HEARTBEAT_INTERVAL) {
        sendDiscovery();
    }
    checkDiscovery();
    if (millis() - _lastHeartbeat > RELAY_TIMEOUT_MS) {
        _relayOnline = false;
    }
}

bool Thermostat::isRelayOnline() { return _relayOnline; }
String Thermostat::getRelayIP() { return _relayIP.toString(); }

bool Thermostat::queryRelayStatus(bool *is_on) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Relè status check KO: WiFi non connesso");
        _relayOnline = false;
        return false;
    }

    HTTPClient http;
    String url = "http://" + _relayIP.toString() + "/status";
    http.setTimeout(1500);
    if (!http.begin(url)) {
        Serial.println("Relè status check KO: begin HTTP fallito");
        _relayOnline = false;
        return false;
    }

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("Relè status check KO: HTTP %d\n", code);
        http.end();
        _relayOnline = false;
        return false;
    }

    const String payload = http.getString();
    http.end();

    bool parsed_on = (payload.indexOf("\"ON\"") >= 0);
    bool parsed_off = (payload.indexOf("\"OFF\"") >= 0);
    if (!parsed_on && !parsed_off) {
        Serial.println("Relè status check KO: payload non valido");
        _relayOnline = false;
        return false;
    }

    _relayOnline = true;
    _lastHeartbeat = millis();
    _lastRelayStatusCheckMs = millis();
    if (is_on) *is_on = parsed_on;
    return true;
}

bool Thermostat::verifyRelayAvailability(bool force) {
    checkHeartbeat(force);
    if (!_relayOnline) return false;

    const unsigned long now = millis();
    if (!force && (now - _lastRelayStatusCheckMs) < RELAY_STATUS_CHECK_INTERVAL_MS) {
        return true;
    }

    bool relay_is_on = false;
    if (!queryRelayStatus(&relay_is_on)) return false;
    isHeating = relay_is_on;
    return true;
}

bool Thermostat::sendRelayCommand(const char *endpoint, bool expected_on) {
    if (!verifyRelayAvailability(true)) {
        Serial.println("Comando relè KO: relè non disponibile");
        return false;
    }

    const unsigned long now = millis();
    if ((now - _lastRelayCmdAttemptMs) < RELAY_CMD_MIN_GAP_MS) {
        delay(RELAY_CMD_MIN_GAP_MS - (now - _lastRelayCmdAttemptMs));
    }
    _lastRelayCmdAttemptMs = millis();

    HTTPClient http;
    String url = "http://" + _relayIP.toString() + endpoint;
    http.setTimeout(1500);
    if (!http.begin(url)) {
        Serial.println("Comando relè KO: begin HTTP fallito");
        _relayOnline = false;
        return false;
    }

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("Comando relè KO: HTTP %d endpoint=%s\n", code, endpoint);
        http.end();
        _relayOnline = false;
        return false;
    }

    const String payload = http.getString();
    http.end();
    if ((expected_on && payload.indexOf("\"ON\"") < 0) ||
        (!expected_on && payload.indexOf("\"OFF\"") < 0)) {
        Serial.printf("Comando relè KO: ack non coerente endpoint=%s payload=%s\n", endpoint, payload.c_str());
        _relayOnline = false;
        return false;
    }

    bool status_on = false;
    if (!queryRelayStatus(&status_on)) return false;
    if (status_on != expected_on) {
        Serial.printf("Comando relè KO: stato finale inatteso endpoint=%s\n", endpoint);
        _relayOnline = false;
        return false;
    }

    Serial.printf("Comando relè OK: endpoint=%s stato=%s\n", endpoint, expected_on ? "ON" : "OFF");
    isHeating = expected_on;
    return true;
}

// ... Logica Boost ...
bool Thermostat::startBoost(int minutes) {
    if (!verifyRelayAvailability(true)) {
        _boostActive = false;
        _boostEndTime = 0;
        return false;
    }

    const bool needs_heat_now = (this->currentTemp < (TARGET_HEAT_ON - TEMP_HYSTERESIS));
    if (needs_heat_now && !startHeating()) {
        _boostActive = false;
        _boostEndTime = 0;
        return false;
    }

    _boostActive = true;
    _boostEndTime = time(NULL) + (minutes * 60);
    _controlMode = ControlMode::AUTO;
    return true;
}

bool Thermostat::stopBoost() {
    _boostActive = false;
    _boostEndTime = 0;
    return true;
}

bool Thermostat::isBoostActive() {
    if (_boostActive && time(NULL) > _boostEndTime) {
        _boostActive = false; 
    }
    return _boostActive;
}

long Thermostat::getBoostRemainingSeconds() {
    if (!isBoostActive()) return 0;
    return (long)difftime(_boostEndTime, time(NULL));
}

void Thermostat::toggleOverride() {
    if (_controlMode == ControlMode::FORCE_OFF) {
        _controlMode = ControlMode::AUTO;
    } else {
        _controlMode = ControlMode::FORCE_OFF;
        stopBoost();
        if (isHeating) {
            (void)stopHeating();
        }
    }
}

bool Thermostat::isOverrideActive() { return _controlMode != ControlMode::AUTO; }

void Thermostat::setControlMode(ControlMode mode) {
    _controlMode = mode;
    if (mode != ControlMode::AUTO) {
        stopBoost();
    }
    if (mode == ControlMode::FORCE_OFF && isHeating) {
        (void)stopHeating();
    }
}

Thermostat::ControlMode Thermostat::getControlMode() const {
    return _controlMode;
}

const char *Thermostat::getControlModeName() const {
    switch (_controlMode) {
        case ControlMode::FORCE_OFF: return "off";
        case ControlMode::FORCE_HEAT: return "heat";
        case ControlMode::MANUAL_TARGET: return "manual";
        default: return "auto";
    }
}

bool Thermostat::startHeating() {
    #ifdef RELAY_PIN
    if (RELAY_PIN >= 0) {
        digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? LOW : HIGH);
        isHeating = true;
        return true;
    }
    #endif
    return sendRelayCommand("/on", true);
}

bool Thermostat::stopHeating() {
    #ifdef RELAY_PIN
    if (RELAY_PIN >= 0) {
        digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
        isHeating = false;
        return true;
    }
    #endif
    return sendRelayCommand("/off", false);
}

float Thermostat::getScheduledTarget() {
    time_t now; 
    time(&now);
    struct tm timeinfo;
    if(!localtime_r(&now, &timeinfo)) return TARGET_HEAT_OFF;

    int day = (timeinfo.tm_wday + 6) % 7; 
    int hour = timeinfo.tm_hour;
    int min = timeinfo.tm_min;
    int slotIndex = hour * 2 + (min >= 30 ? 1 : 0);
    
    bool isComfort = (configManager.data.weekSchedule[day].timeSlots >> slotIndex) & 1ULL;
    return isComfort ? TARGET_HEAT_ON : TARGET_HEAT_OFF;
}

void Thermostat::run() {
    readSensors(); 
    checkHeartbeat(); 
    (void)verifyRelayAvailability(false);
    const bool boostActive = isBoostActive();

    if (boostActive) {
        targetTemp = TARGET_HEAT_ON;
    } else {
        switch (_controlMode) {
            case ControlMode::FORCE_HEAT:
                targetTemp = TARGET_HEAT_ON;
                break;
            case ControlMode::FORCE_OFF:
                targetTemp = TARGET_HEAT_OFF;
                break;
            case ControlMode::MANUAL_TARGET:
                targetTemp = _manualTargetTemp;
                break;
            case ControlMode::AUTO:
            default:
                targetTemp = getScheduledTarget();
                break;
        }
    }

    if (this->currentTemp < (targetTemp - TEMP_HYSTERESIS)) {
        if (!isHeating) startHeating();
    } 
    else if (this->currentTemp > (targetTemp + TEMP_HYSTERESIS)) {
        if (isHeating) stopHeating();
    }
}

void Thermostat::update(float temp) {
    this->currentTemp = temp;
}

void Thermostat::setTarget(float target) { 
    if (!isnan(target) && target >= 10.0f && target <= 35.0f) {
        _manualTargetTemp = target;
        _controlMode = ControlMode::MANUAL_TARGET;
        stopBoost();
    }
}

float Thermostat::getTarget() {
    if (isBoostActive()) return TARGET_HEAT_ON;
    if (_controlMode == ControlMode::MANUAL_TARGET) return _manualTargetTemp;
    return targetTemp;
}
