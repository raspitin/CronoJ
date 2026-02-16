#ifndef CONFIG_H
#define CONFIG_H

// --- HARDWARE PIN ---
// Metti -1 se non hai un relè collegato fisicamente al display
#define RELAY_PIN -1  

// --- CONFIGURAZIONE RELÈ REMOTO ---
// IP di fallback se l'auto-discovery fallisce
#define DEFAULT_REMOTE_RELAY_IP "192.168.1.33" 

// Configurazione Discovery UDP
#define DISCOVERY_PORT 9876
#define DISCOVERY_PACKET_CONTENT "RELAY_HERE_V1"

// --- LOGGING ---
#define SYSLOG_SERVER "192.168.1.140" 
#define SYSLOG_PORT 514
#define DEVICE_NAME "Termostato_Smart"
#define DEVICE_HOSTNAME "cronoj"

// --- OTA SECURITY ---
// Cambia queste credenziali prima dell'uso in produzione.
#define OTA_WEB_USERNAME "admin"
#define OTA_WEB_PASSWORD "ChangeMe_OTA_Web"
#define OTA_ARDUINO_PASSWORD "ChangeMe_OTA_PIO"
#define OTA_ARDUINO_PORT 3232

// --- PARAMETRI TERMOSTATO ---
#define TEMP_HYSTERESIS 0.2
#define TARGET_HEAT_ON  22.0
#define TARGET_HEAT_OFF 16.0

// --- ALTRI ---
#define RELAY_ACTIVE_LOW true 

// --- WIFI SETUP AP ---
#define WIFI_SETUP_AP_SSID "Termostato_Setup"
// Vuoto = AP aperto. Se impostata, usare una password WPA2 di almeno 8 caratteri.
#define WIFI_SETUP_AP_PASSWORD ""

// --- MQTT CONFIG ---
#define MQTT_SERVER "192.168.1.140" // IP del tuo Geekom Air 12
#define MQTT_PORT 1883
#define MQTT_USER ""              // Lascia vuoto se non hai messo password su Mosquitto
#define MQTT_PASS ""
// Topic
#define MQTT_TOPIC_STATE  "home/thermostat/state"        // heating on/off
#define MQTT_TOPIC_TEMP   "home/thermostat/temperature"  // temp attuale
#define MQTT_TOPIC_TARGET "home/thermostat/target"       // temp target (lettura)
#define MQTT_TOPIC_SET    "home/thermostat/target/set"   // temp target (scrittura)
#define MQTT_TOPIC_MODE   "home/thermostat/mode/set"     // heat/off/auto

#endif
