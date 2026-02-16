#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Inizializza il server web
void setup_web_server();
void web_interface_loop();

#endif
