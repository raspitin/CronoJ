#include "network_manager.h"
#include "config_manager.h"
#include "config.h"
#include <WiFiManager.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "ui.h" 
#include <cstring>

static bool mdns_started = false;

bool shouldSaveConfig = false;

void saveConfigCallback() {
    Serial.println("Configurazione modificata nel portale -> Salvataggio richiesto.");
    shouldSaveConfig = true;
}

// Funzione helper per rimuovere spazi iniziali e finali
String trimString(const char* str) {
    String s = String(str);
    s.trim();
    return s;
}

bool network_start_mdns() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    if (mdns_started) {
        return true;
    }
    if (!MDNS.begin(DEVICE_HOSTNAME)) {
        Serial.println("mDNS start failed");
        return false;
    }
    MDNS.addService("http", "tcp", 80);
    mdns_started = true;
    Serial.printf("mDNS attivo: http://%s.local\n", DEVICE_HOSTNAME);
    return true;
}

void network_stop_mdns() {
    if (!mdns_started) return;
    MDNS.end();
    mdns_started = false;
}

bool setup_network() {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(DEVICE_HOSTNAME);

    WiFiManager wm;
    wm.setHostname(DEVICE_HOSTNAME);

    // Parametri Custom
    WiFiManagerParameter custom_wkey("wkey", "OpenWeather API Key", configManager.data.weatherKey, 64);
    WiFiManagerParameter custom_city("city", "Citta (es. Rome)", configManager.data.weatherCity, 32);
    WiFiManagerParameter custom_country("country", "Paese (es. IT)", configManager.data.weatherCountry, 4);
    WiFiManagerParameter custom_tz("tz", "Timezone", configManager.data.timezone, 64);

    wm.addParameter(&custom_wkey);
    wm.addParameter(&custom_city);
    wm.addParameter(&custom_country);
    wm.addParameter(&custom_tz);

    // CALLBACK: Se non trova WiFi e apre l'AP, aggiorna la Splash Screen con QR e istruzioni
    wm.setAPCallback([](WiFiManager *myWiFiManager) {
        (void)myWiFiManager;
        Serial.println("Modalità Configurazione AP Attiva");
        // Avoid LVGL calls from WiFiManager callback context to prevent races/panics.
    });

    wm.setSaveConfigCallback(saveConfigCallback);
    wm.setDebugOutput(true);
    wm.setConfigPortalTimeout(180); 

    // Avvio connessione automatica
    bool res = false;
    if (strlen(WIFI_SETUP_AP_PASSWORD) >= 8) {
        res = wm.autoConnect(WIFI_SETUP_AP_SSID, WIFI_SETUP_AP_PASSWORD);
    } else {
        res = wm.autoConnect(WIFI_SETUP_AP_SSID);
    }

    if (!res) {
        Serial.println("Connessione fallita o timeout scaduto.");
        return false;
    }

    Serial.println("WiFi Connesso!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.printf("Hostname: %s\n", DEVICE_HOSTNAME);

    if (shouldSaveConfig) {
        // TRIM dei valori per rimuovere spazi indesiderati
        String wkey = trimString(custom_wkey.getValue());
        String city = trimString(custom_city.getValue());
        String country = trimString(custom_country.getValue());
        String tz = trimString(custom_tz.getValue());
        
        strlcpy(configManager.data.weatherKey, wkey.c_str(), sizeof(configManager.data.weatherKey));
        strlcpy(configManager.data.weatherCity, city.c_str(), sizeof(configManager.data.weatherCity));
        strlcpy(configManager.data.weatherCountry, country.c_str(), sizeof(configManager.data.weatherCountry));
        strlcpy(configManager.data.timezone, tz.c_str(), sizeof(configManager.data.timezone));
        
        Serial.printf("Salvati: City='%s' Country='%s'\n", city.c_str(), country.c_str());
        
        configManager.saveConfig();
    }

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", configManager.data.timezone, 1);
    tzset();

    network_start_mdns();

    return true;
}

void wifi_reset_settings() {
    network_stop_mdns();
    WiFiManager wm;
    wm.resetSettings();
    Serial.println("Impostazioni WiFi cancellate.");
    delay(1000);
    ESP.restart();
}
