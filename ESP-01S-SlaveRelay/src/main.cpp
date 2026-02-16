#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <WiFiManager.h> 
#include "secrets.h"

#define RELAY_PIN 0 
#define DISCOVERY_PORT 9876
#define DISCOVERY_MSG "RELAY_HERE_V1"

ESP8266WebServer server(80);
WiFiUDP udp;

unsigned long lastBroadcast = 0;

void handleOn() {
  digitalWrite(RELAY_PIN, LOW); // Attivo
  server.send(200, "application/json", "{\"status\":\"ON\"}");
}

void handleOff() {
  digitalWrite(RELAY_PIN, HIGH); // Spento
  server.send(200, "application/json", "{\"status\":\"OFF\"}");
}

void handleStatus() {
  int state = digitalRead(RELAY_PIN);
  String s = (state == LOW) ? "ON" : "OFF";
  server.send(200, "application/json", "{\"status\":\"" + s + "\"}");
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Default OFF

  WiFiManager wm;
  // Rimuoviamo config statica per usare DHCP (più flessibile con Auto-Discovery)
  // wm.setSTAStaticIPConfig(...); <--- RIMOSSO
  wm.setConfigPortalTimeout(180);
  
  if (!wm.autoConnect("Relay_Setup")) {
    ESP.restart();
  }

  server.on("/on", handleOn);
  server.on("/off", handleOff);
  server.on("/status", handleStatus);
  server.begin();

  // Avvia UDP per Discovery
  udp.begin(DISCOVERY_PORT);
  Serial.println("Relè Pronto. IP: " + WiFi.localIP().toString());
}

void loop() {
  server.handleClient();

  // Invia "Sono Qui" ogni 2 secondi
  if (millis() - lastBroadcast > 2000) {
    // Broadcast IP: 255.255.255.255 invia a tutti nella rete locale
    udp.beginPacket(IPAddress(255,255,255,255), DISCOVERY_PORT);
    udp.write(DISCOVERY_MSG);
    udp.endPacket();
    lastBroadcast = millis();
  }
}