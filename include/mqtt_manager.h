#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H
#include <Arduino.h>

void mqtt_setup();
void mqtt_loop();
void mqtt_publish_state(float currentTemp, float targetTemp, bool isHeating);

#endif