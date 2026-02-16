# Smart Thermostat ESP32-S3 (JC8048W550)

Termostato smart basato su ESP32-S3 con display touch JC8048W550 (800x480), interfaccia LVGL, gestione relè locale/remoto, programmazione settimanale, meteo, MQTT, InfluxDB e aggiornamento firmware OTA.

## Caratteristiche principali

- UI touch con LVGL 9 ottimizzata per pannello RGB ST7262.
- Anti-tearing e tuning touch per miglior fluidita e risposta.
- Programmazione settimanale con slot da 30 minuti.
- Modalita boost temporizzata con verifica reale dello stato relè.
- Supporto relè locale su GPIO o relè remoto (ESP-01S) via rete.
- Discovery relè remoto via broadcast UDP + heartbeat.
- Controllo relè remoto con endpoint HTTP (`/on`, `/off`, `/status`).
- Meteo da OpenWeather (URL API configurabile).
- Dashboard e setup via web (stato, config, schedule, OTA).
- Telemetria MQTT e InfluxDB.
- mDNS attivo: hostname locale `cronoj.local`.
- OTA firmware via web e via PlatformIO (`espota`) con password.

## Architettura rapida

- `src/main.cpp`: loop principale, task meteo, watchdog, integrazione moduli.
- `src/ui.cpp`: logica UI LVGL.
- `src/thermostat.cpp`: logica termostato, relè e heartbeat/discovery.
- `src/web_interface.cpp`: dashboard/setup web e OTA web.
- `src/ota_manager.cpp`: OTA Arduino per upload da PlatformIO via rete.
- `src/config_manager.cpp`: persistenza configurazione in LittleFS (`/config.json`).

## Cartella include

La cartella `include/` contiene solo header necessari alla build. Verifica fatta su include diretti nel codice e su include indiretti (LVGL):

- `config.h`
- `config_manager.h`
- `influx_manager.h`
- `lv_conf.h` (necessario via `LV_CONF_INCLUDE_SIMPLE` in `platformio.ini`)
- `mqtt_manager.h`
- `network_manager.h`
- `ota_manager.h`
- `secrets.h`
- `thermostat.h`
- `ui.h`
- `web_interface.h`

## Requisiti

### Hardware

- Board JC8048W550 (ESP32-S3 + display touch RGB).
- Sensori I2C supportati nel progetto (AHT20 + BMP280).
- Opzionale: relè remoto ESP-01S con firmware slave.

### Software

- Visual Studio Code
- Estensione PlatformIO

## Configurazione

Parametri principali in `include/config.h`:

- `DEVICE_HOSTNAME` (default: `cronoj`)
- `RELAY_PIN` (`-1` = solo relè remoto)
- `DEFAULT_REMOTE_RELAY_IP`
- `DISCOVERY_PORT`, `DISCOVERY_PACKET_CONTENT`
- `MQTT_*`
- credenziali OTA:
  - `OTA_WEB_USERNAME`
  - `OTA_WEB_PASSWORD`
  - `OTA_ARDUINO_PASSWORD`
  - `OTA_ARDUINO_PORT`

Configurazione runtime salvata in LittleFS (web/captive portal):

- WiFi
- OpenWeather key/url/citta/paese
- timezone
- InfluxDB URL/org/bucket/token
- schedule settimanale

## Build e upload USB

```bash
pio run
pio run -e jc8048w550 -t upload
```

## OTA firmware

Il progetto supporta due modalita OTA.

### 1) OTA via Web UI

Nel tab Setup della dashboard web:

- inserisci `OTA User` e `OTA Password`
- seleziona il file `.bin`
- avvia upload

Endpoint usato: `POST /api/ota` con Basic Auth.

Esempio CLI (PowerShell):

```powershell
curl.exe -u "admin:ChangeMe_OTA_Web" -X POST -F "firmware=@.pio/build/jc8048w550/firmware.bin" http://cronoj.local/api/ota
```

### 2) OTA da PlatformIO (espota)

Ambiente dedicato in `platformio.ini`: `jc8048w550_ota`.

Prima di lanciare l'upload, imposta:

- `JC8048W550_OTA_PASS`: password OTA Arduino (`OTA_ARDUINO_PASSWORD`)
- `JC8048W550_OTA_HOST`: IP del tuo PC sulla stessa LAN del dispositivo (non l'IP dell'ESP)

Esempio (PowerShell):

```powershell
$env:JC8048W550_OTA_PASS="ChangeMe_OTA_PIO"
$env:JC8048W550_OTA_HOST="192.168.1.200"
pio run -e jc8048w550_ota -t upload
```

Note:

- L'ESP espone OTA su `cronoj.local:3232` (porta configurabile).
- Se `JC8048W550_OTA_HOST` non e impostata correttamente, `espota` puo fallire con `No response from device`.

## Primo avvio

Se non trova una rete salvata, il dispositivo apre AP di configurazione:

- SSID: `Termostato_Setup`
- password AP: da `WIFI_SETUP_AP_PASSWORD` (vuota = AP aperto)

Da captive portal puoi impostare WiFi e parametri base.

## API web principali

- `GET /api/status`
- `GET /api/config`
- `POST /api/config`
- `GET /api/schedule`
- `POST /api/schedule`
- `POST /api/ota` (Basic Auth)

## Troubleshooting rapido

- Tearing/artifacts: controlla i flag display in `platformio.ini`.
- OTA PlatformIO non parte:
  - verifica che `cronoj.local` risolva
  - imposta `JC8048W550_OTA_HOST` con IP LAN del PC
  - verifica password OTA
  - controlla firewall locale
- Relè remoto offline:
  - verifica heartbeat e endpoint `/status`
  - verifica IP fallback e rete locale

## Sicurezza

Prima della messa in esercizio:

- cambia tutte le password OTA di default in `include/config.h`
- evita credenziali deboli
- limita accesso LAN al dispositivo

## Licenza

Questo progetto e distribuito con licenza `PolyForm-Noncommercial-1.0.0` (vedi file `LICENSE`).

Uso commerciale non consentito senza accordo separato con l'autore.
