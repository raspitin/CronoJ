#include "mqtt_manager.h"
#include "config.h"
#include "thermostat.h"
#include <WiFi.h>
#include <PubSubClient.h>

extern Thermostat thermo;
WiFiClient espClient;
PubSubClient mqttClient(espClient); // <--- RINOMINATO DA client A mqttClient

unsigned long lastReconnectAttempt = 0;

void callback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.printf("MQTT Rx [%s]: %s\n", topic, message.c_str());

    // Cambio Target Temperatura da remoto
    if (String(topic) == MQTT_TOPIC_SET) {
        float newTarget = message.toFloat();
        if (newTarget > 10 && newTarget < 35) {
            thermo.setTarget(newTarget);
            Serial.printf("MQTT target manuale impostato: %.1f\n", newTarget);
        }
    }
    // Modalita' da remoto: heat/off/auto
    else if (String(topic) == MQTT_TOPIC_MODE) {
        if (message == "heat") thermo.setControlMode(Thermostat::ControlMode::FORCE_HEAT);
        else if (message == "off") thermo.setControlMode(Thermostat::ControlMode::FORCE_OFF);
        else if (message == "auto") thermo.setControlMode(Thermostat::ControlMode::AUTO);
    }
}

boolean reconnect() {
    if (mqttClient.connect("ESP32Thermostat", MQTT_USER, MQTT_PASS)) { // <--- mqttClient
        Serial.println("MQTT Connesso");
        // Sottoscrizione ai comandi
        mqttClient.subscribe(MQTT_TOPIC_SET);  // <--- mqttClient
        mqttClient.subscribe(MQTT_TOPIC_MODE); // <--- mqttClient
    }
    return mqttClient.connected(); // <--- mqttClient
}

void mqtt_setup() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT); // <--- mqttClient
    mqttClient.setCallback(callback);             // <--- mqttClient
}

void mqtt_loop() {
    if (!mqttClient.connected()) { // <--- mqttClient
        long now = millis();
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            if (reconnect()) {
                lastReconnectAttempt = 0;
            }
        }
    } else {
        mqttClient.loop(); // <--- mqttClient
    }
}

void mqtt_publish_state(float currentTemp, float targetTemp, bool isHeating) {
    if (!mqttClient.connected()) return; // <--- mqttClient
    
    mqttClient.publish(MQTT_TOPIC_TEMP, String(currentTemp, 1).c_str());   // <--- mqttClient
    // Pubblica Umidità
    mqttClient.publish("home/thermostat/humidity", String(thermo.getHumidity(), 0).c_str()); // <--- mqttClient
    
    mqttClient.publish(MQTT_TOPIC_TARGET, String(targetTemp, 1).c_str()); // <--- mqttClient
    mqttClient.publish(MQTT_TOPIC_STATE, isHeating ? "heating" : "off");  // <--- mqttClient
}
