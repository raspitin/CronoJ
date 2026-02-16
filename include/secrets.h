#ifndef SECRETS_H
#define SECRETS_H

// Lista di 60 token esadecimali (uno per ogni minuto dell'ora 0-59)
// Questa lista DEVE essere identica su Master (Display) e Slave (Relè)
static const char* SECRET_TOKENS[60] = {
    "2d8f9a4c7e3b1d6f", "5a1b3c9d8e2f4a07", "e7c9b2a5d8f31046", "9046a3c2d1e8f7b5",
    "b5f8e2d1c9a30746", "1c4d7e2f5a8b9036", "6a9b3c2d5f8e1704", "f3e2d1c9b8a70564",
    "8b2a5d9c1e4f7036", "4d7e1b2a5c8f9063", "0f3e9c2d5a8b1746", "c9a3b2d1e8f70564",
    "7e2f5a1b3c9d8046", "3c9d8e2f5a1b7064", "a5d8f3c9b2e14076", "d1e8f7c9a3b20546",
    "5a8b1c4d7e2f9036", "9b3c6a2d5f8e1704", "2d1c9f3e8a7b0564", "e4f78b2a5d9c1036",
    "a5c84d7e1b2f9063", "d5a80f3e9c2b1746", "f705c9a3b2d1e864", "d8047e2f5a1b3c96",
    "b7063c9d8e2f5a14", "e140a5d8f3c9b276", "b205d1e8f7c9a346", "f9035a8b1c4d7e26",
    "e1709b3c6a2d5f84", "7b052d1c9f3e8a64", "c103e4f78b2a5d96", "f906a5c84d7e1b23",
    "b174d5a80f3e9c26", "e864f705c9a3b2d1", "b3c9d8047e2f5a16", "f5a1b7063c9d8e24",
    "9b27e140a5d8f3c6", "9a34b205d1e8f7c6", "d7e2f9035a8b1c46", "d5f8e1709b3c6a24",
    "e8a67b052d1c9f34", "a5d9c103e4f78b26", "e1b2f906a5c84d73", "e9c2b174d5a80f36",
    "3b2de864f705c9a1", "2f5ab3c9d8047e26", "9d8ef5a1b7063c94", "8f3c9b27e140a5d6",
    "8f7c9a34b205d1e6", "b1c4d7e2f9035a86", "c6a2d5f8e1709b34", "c9f3e8a67b052d14",
    "78b2a5d9c103e4f6", "84d7e1b2f906a5c3", "80f3e9c2b174d5a6", "5c9a3b2de864f701",
    "47e2f5ab3c9d8046", "63c9d8ef5a1b7064", "40a5d8f3c9b27e16", "5d1e8f7c9a34b206"
};

#define INFLUXDB_URL "http://192.168.1.140:8087"
#define INFLUXDB_TOKEN "eVcWV34_JdhJLeuMlelTRvf1XEJPgwlYeTsoURItxgAY2FYueDNEg2Y8IEaGPdGTXOMI1UhAxHCJtLXSyiJkFw=="
#define INFLUXDB_ORG "home_automation"
#define INFLUXDB_BUCKET "termostato_data"

#endif