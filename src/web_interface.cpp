#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include <cstdlib>
#include <cmath>
#include "web_interface.h"
#include "config_manager.h"
#include "thermostat.h"
#include "config.h"

extern Thermostat thermo;

AsyncWebServer server(80);

static bool restart_pending = false;
static unsigned long restart_at_ms = 0;

static const char *index_html = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Termostato Smart</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f4f4f9; color: #333; }
    .header { background-color: #007bff; color: white; padding: 15px; text-align: center; }
    .tabs { display: flex; background: #ddd; }
    .tab { flex: 1; padding: 15px; text-align: center; cursor: pointer; border-bottom: 3px solid transparent; }
    .tab.active { border-bottom: 3px solid #007bff; background: #fff; font-weight: bold; }
    .content { padding: 20px; display: none; }
    .content.active { display: block; }
    .card { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); margin-bottom: 20px; }
    h2 { margin-top: 0; }
    label { display: block; margin-top: 10px; font-weight: bold; }
    input, select { width: 100%; padding: 8px; margin-top: 5px; box-sizing: border-box; border: 1px solid #ccc; border-radius: 4px; }
    button { background-color: #007bff; color: white; border: none; padding: 10px 20px; cursor: pointer; border-radius: 4px; margin-top: 15px; }
    button:hover { background-color: #0056b3; }

    .schedule-wrap { overflow-x: auto; }
    .schedule-grid { display: grid; grid-template-columns: 80px repeat(48, minmax(32px, 1fr)); gap: 2px; min-width: 1900px; }
    .sched-cell { height: 30px; border: 1px solid #eee; text-align: center; font-size: 10px; line-height: 30px; cursor: pointer; user-select: none; }
    .sched-header { font-weight: bold; background: #eee; }
    .sched-day { font-weight: bold; background: #eee; line-height: 30px; padding-left: 5px; }
    .mode-heat { background-color: #ffcccc; }
    .mode-off { background-color: #f0f0f0; }
    #ota_progress { width: 100%; height: 18px; margin-top: 10px; }
    #ota_status { margin-top: 8px; color: #555; }
  </style>
</head>
<body>
  <div class="header"><h1>Termostato Control</h1></div>
  <div class="tabs">
    <div class="tab active" onclick="openTab('dashboard', this)">Dashboard</div>
    <div class="tab" onclick="openTab('schedule', this)">Programma</div>
    <div class="tab" onclick="openTab('setup', this)">Setup</div>
  </div>

  <div id="dashboard" class="content active">
    <div class="card">
      <h2>Stato Attuale</h2>
      <p>Temperatura: <span id="val_temp">--</span> &deg;C</p>
      <p>Target: <span id="val_target">--</span> &deg;C</p>
      <p>Umidit&agrave;: <span id="val_hum">--</span> %</p>
      <p>Pressione: <span id="val_press">--</span> hPa</p>
      <p>Stato Rel&egrave;: <span id="val_relay">--</span></p>
    </div>
  </div>

  <div id="schedule" class="content">
    <div class="card">
      <h2>Programmazione Settimanale (slot da 30 minuti)</h2>
      <p>Clicca per attivare/disattivare (Rosso = Comfort / Acceso)</p>
      <div class="schedule-wrap">
        <div id="grid_container" class="schedule-grid"></div>
      </div>
      <button onclick="saveSchedule()">Salva Programmazione</button>
    </div>
  </div>

  <div id="setup" class="content">
    <div class="card">
      <h2>InfluxDB Config</h2>
      <label>Server URL:</label> <input type="text" id="inf_url">
      <label>Organization:</label> <input type="text" id="inf_org">
      <label>Bucket:</label> <input type="text" id="inf_bucket">
      <label>Token:</label> <input type="password" id="inf_token">
    </div>
    <div class="card">
      <h2>API & Calibrazione</h2>
      <label>Meteo API URL:</label> <input type="text" id="weather_url">
      <label>OpenWeather API Key:</label> <input type="text" id="api_key">
      <label>Temp Offset:</label> <input type="number" step="0.1" id="temp_offset">
      <button onclick="saveConfig()">Salva Configurazione</button>
    </div>
    <div class="card">
      <h2>Aggiornamento Firmware OTA</h2>
      <p>Hostname locale: <b>cronoj.local</b></p>
      <label>OTA User:</label> <input type="text" id="ota_user" placeholder="admin">
      <label>OTA Password:</label> <input type="password" id="ota_pass" placeholder="password OTA">
      <input type="file" id="ota_file" accept=".bin,application/octet-stream">
      <button onclick="uploadFirmware()">Carica Firmware</button>
      <progress id="ota_progress" value="0" max="100"></progress>
      <div id="ota_status">Inserisci credenziali OTA, seleziona il file .bin e avvia il caricamento.</div>
    </div>
  </div>

<script>
  const DAY_NAMES = ["Lun", "Mar", "Mer", "Gio", "Ven", "Sab", "Dom"];
  const SLOT_COUNT = 48;
  let scheduleData = Array.from({ length: 7 }, () => Array(SLOT_COUNT).fill(false));

  function openTab(id, tabEl) {
    document.querySelectorAll('.content').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.tab').forEach(el => el.classList.remove('active'));
    document.getElementById(id).classList.add('active');
    tabEl.classList.add('active');
  }

  function slotLabel(slotIndex) {
    const h = String(Math.floor(slotIndex / 2)).padStart(2, '0');
    const m = slotIndex % 2 === 0 ? '00' : '30';
    return `${h}:${m}`;
  }

  function setCellVisual(cell, isOn) {
    cell.classList.remove('mode-heat', 'mode-off');
    cell.classList.add(isOn ? 'mode-heat' : 'mode-off');
  }

  function initGrid() {
    const grid = document.getElementById('grid_container');
    grid.innerHTML = "";

    const blank = document.createElement('div');
    blank.className = 'sched-cell sched-header';
    blank.innerText = 'Giorno';
    grid.appendChild(blank);

    for (let s = 0; s < SLOT_COUNT; s++) {
      const h = document.createElement('div');
      h.className = 'sched-cell sched-header';
      h.innerText = slotLabel(s);
      grid.appendChild(h);
    }

    for (let d = 0; d < 7; d++) {
      const day = document.createElement('div');
      day.className = 'sched-day';
      day.innerText = DAY_NAMES[d];
      grid.appendChild(day);

      for (let s = 0; s < SLOT_COUNT; s++) {
        const c = document.createElement('div');
        c.className = 'sched-cell mode-off';
        c.dataset.day = d;
        c.dataset.slot = s;
        c.title = `${DAY_NAMES[d]} ${slotLabel(s)}`;
        c.onclick = function() {
          const dayIdx = Number(this.dataset.day);
          const slotIdx = Number(this.dataset.slot);
          scheduleData[dayIdx][slotIdx] = !scheduleData[dayIdx][slotIdx];
          setCellVisual(this, scheduleData[dayIdx][slotIdx]);
        };
        grid.appendChild(c);
      }
    }
  }

  function renderSchedule() {
    document.querySelectorAll('.sched-cell[data-day]').forEach(cell => {
      const d = Number(cell.dataset.day);
      const s = Number(cell.dataset.slot);
      setCellVisual(cell, !!scheduleData[d][s]);
    });
  }

  function unpackDayMask(dayMask) {
    const row = Array(SLOT_COUNT).fill(false);
    let mask = BigInt(dayMask.toString());
    for (let s = 0; s < SLOT_COUNT; s++) {
      row[s] = ((mask >> BigInt(s)) & 1n) === 1n;
    }
    return row;
  }

  function packDayMask(row) {
    let mask = 0n;
    for (let s = 0; s < SLOT_COUNT; s++) {
      if (row[s]) mask |= (1n << BigInt(s));
    }
    return mask.toString();
  }

  function formatOneDecimal(v) {
    const n = Number(v);
    return Number.isFinite(n) ? n.toFixed(1) : "--";
  }

  function fetchStatus() {
    fetch('/api/status').then(r => r.json()).then(d => {
      document.getElementById('val_temp').innerText = formatOneDecimal(d.temp);
      document.getElementById('val_target').innerText = formatOneDecimal(d.target);
      document.getElementById('val_hum').innerText = formatOneDecimal(d.hum);
      document.getElementById('val_press').innerText = formatOneDecimal(d.press);
      document.getElementById('val_relay').innerText = d.relay ? "ON" : "OFF";
    });
  }

  function fetchConfig() {
    fetch('/api/config').then(r => r.json()).then(d => {
      document.getElementById('inf_url').value = d.influxUrl || "";
      document.getElementById('inf_org').value = d.influxOrg || "";
      document.getElementById('inf_bucket').value = d.influxBucket || "";
      document.getElementById('inf_token').value = d.influxToken || "";
      document.getElementById('weather_url').value = d.weatherApiUrl || "";
      document.getElementById('api_key').value = d.weatherKey || "";
      document.getElementById('temp_offset').value = d.tempOffset || 0;
    });
  }

  function fetchSched() {
    fetch('/api/schedule').then(r => r.json()).then(d => {
      const src = Array.isArray(d.schedule) ? d.schedule : [];
      for (let day = 0; day < 7; day++) {
        scheduleData[day] = unpackDayMask(src[day] || "0");
      }
      renderSchedule();
    });
  }

  function saveConfig() {
    const payload = {
      influxUrl: document.getElementById('inf_url').value,
      influxOrg: document.getElementById('inf_org').value,
      influxBucket: document.getElementById('inf_bucket').value,
      influxToken: document.getElementById('inf_token').value,
      weatherApiUrl: document.getElementById('weather_url').value,
      weatherKey: document.getElementById('api_key').value,
      tempOffset: parseFloat(document.getElementById('temp_offset').value || "0")
    };
    fetch('/api/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    }).then(r => {
      if (r.ok) alert("Configurazione salvata. Riavvio in corso...");
      else alert("Errore salvataggio configurazione");
    });
  }

  function saveSchedule() {
    const payload = { schedule: scheduleData.map(packDayMask) };
    fetch('/api/schedule', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    }).then(r => {
      if (r.ok) alert("Programmazione salvata");
      else alert("Errore salvataggio programmazione");
    });
  }

  function uploadFirmware() {
    const fileInput = document.getElementById('ota_file');
    const progress = document.getElementById('ota_progress');
    const status = document.getElementById('ota_status');
    const otaUser = document.getElementById('ota_user').value.trim();
    const otaPass = document.getElementById('ota_pass').value;
    if (!fileInput.files || fileInput.files.length === 0) {
      status.innerText = "Seleziona prima un firmware .bin";
      return;
    }
    if (!otaUser || !otaPass) {
      status.innerText = "Inserisci OTA user e password";
      return;
    }

    const file = fileInput.files[0];
    const formData = new FormData();
    formData.append('firmware', file, file.name);

    progress.value = 0;
    status.innerText = "Upload in corso...";

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/ota', true);
    xhr.setRequestHeader('Authorization', 'Basic ' + btoa(otaUser + ":" + otaPass));
    xhr.upload.onprogress = function(e) {
      if (!e.lengthComputable) return;
      progress.value = Math.round((e.loaded / e.total) * 100);
    };
    xhr.onload = function() {
      if (xhr.status === 200) {
        progress.value = 100;
        status.innerText = "OTA completato. Riavvio dispositivo...";
      } else if (xhr.status === 401) {
        status.innerText = "Credenziali OTA non valide";
      } else {
        status.innerText = "Errore OTA: " + xhr.responseText;
      }
    };
    xhr.onerror = function() {
      status.innerText = "Errore rete durante upload OTA";
    };
    xhr.send(formData);
  }

  initGrid();
  fetchStatus();
  fetchConfig();
  fetchSched();
  setInterval(fetchStatus, 5000);
</script>
</body></html>
)rawliteral";

static String *get_body_buffer(AsyncWebServerRequest *request, size_t total) {
    if (request->_tempObject == nullptr) {
        String *body = new String();
        body->reserve(total + 1);
        request->_tempObject = body;
    }
    return reinterpret_cast<String *>(request->_tempObject);
}

static void free_body_buffer(AsyncWebServerRequest *request) {
    if (request->_tempObject != nullptr) {
        delete reinterpret_cast<String *>(request->_tempObject);
        request->_tempObject = nullptr;
    }
}

static bool parse_slot_mask(JsonVariantConst value, uint64_t &out_mask) {
    if (value.is<const char *>()) {
        const char *s = value.as<const char *>();
        if (!s) return false;
        char *end_ptr = nullptr;
        unsigned long long parsed = strtoull(s, &end_ptr, 10);
        if (end_ptr == s) return false;
        out_mask = static_cast<uint64_t>(parsed);
        return true;
    }
    if (value.is<uint64_t>()) {
        out_mask = value.as<uint64_t>();
        return true;
    }
    if (value.is<unsigned long>()) {
        out_mask = static_cast<uint64_t>(value.as<unsigned long>());
        return true;
    }
    return false;
}

static bool is_ota_authorized(AsyncWebServerRequest *request) {
    if (OTA_WEB_USERNAME[0] == '\0' || OTA_WEB_PASSWORD[0] == '\0') {
        return true;
    }
    return request->authenticate(OTA_WEB_USERNAME, OTA_WEB_PASSWORD);
}

void setup_web_server() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", index_html);
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(512);
        auto round1 = [](float v) {
            return std::round(v * 10.0f) / 10.0f;
        };
        doc["temp"] = round1(thermo.getCurrentTemp());
        doc["hum"] = round1(thermo.getHumidity());
        doc["press"] = round1(thermo.getPressure());
        doc["target"] = round1(thermo.getTarget());
        doc["relay"] = thermo.isHeatingState();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(1024);
        doc["ssid"] = WiFi.SSID();
        doc["ip"] = WiFi.localIP().toString();
        doc["weatherKey"] = configManager.data.weatherKey;
        doc["weatherApiUrl"] = configManager.data.weatherApiUrl;
        doc["influxUrl"] = configManager.data.influxUrl;
        doc["influxOrg"] = configManager.data.influxOrg;
        doc["influxBucket"] = configManager.data.influxBucket;
        doc["influxToken"] = configManager.data.influxToken;
        doc["tempOffset"] = configManager.data.tempOffset;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/api/schedule", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(4096);
        JsonArray sched = doc.createNestedArray("schedule");
        for (int d = 0; d < 7; d++) {
            sched.add(String(configManager.data.weekSchedule[d].timeSlots));
        }
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/api/ota", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            if (!is_ota_authorized(request)) {
                request->requestAuthentication();
                return;
            }
            if (Update.hasError()) {
                request->send(500, "text/plain", "OTA failed");
                return;
            }
            restart_pending = true;
            restart_at_ms = millis() + 1200;
            request->send(200, "text/plain", "OTA OK, rebooting");
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!is_ota_authorized(request)) {
                return;
            }
            if (index == 0) {
                Serial.printf("OTA upload start: %s\n", filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                }
            }

            if (len > 0) {
                if (Update.write(data, len) != len) {
                    Update.printError(Serial);
                }
            }

            if (final) {
                if (Update.end(true)) {
                    Serial.printf("OTA upload success: %u bytes\n", (unsigned)(index + len));
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );

    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (request->method() != HTTP_POST) return;
        const String url = request->url();
        if (url != "/api/config" && url != "/api/schedule") return;

        if (index == 0) {
            free_body_buffer(request);
        }

        String *body = get_body_buffer(request, total);
        body->concat(reinterpret_cast<const char *>(data), len);
        if ((index + len) < total) return;

        if (url == "/api/config") {
            StaticJsonDocument<1024> doc;
            DeserializationError error = deserializeJson(doc, *body);
            if (error) {
                request->send(400, "text/plain", "Invalid JSON");
                free_body_buffer(request);
                return;
            }

            strlcpy(configManager.data.weatherKey, doc["weatherKey"] | "", sizeof(configManager.data.weatherKey));
            strlcpy(configManager.data.weatherApiUrl, doc["weatherApiUrl"] | "", sizeof(configManager.data.weatherApiUrl));
            strlcpy(configManager.data.influxUrl, doc["influxUrl"] | "", sizeof(configManager.data.influxUrl));
            strlcpy(configManager.data.influxOrg, doc["influxOrg"] | "", sizeof(configManager.data.influxOrg));
            strlcpy(configManager.data.influxBucket, doc["influxBucket"] | "", sizeof(configManager.data.influxBucket));
            strlcpy(configManager.data.influxToken, doc["influxToken"] | "", sizeof(configManager.data.influxToken));
            configManager.data.tempOffset = doc["tempOffset"] | 0.0f;

            if (configManager.saveConfig()) {
                restart_pending = true;
                restart_at_ms = millis() + 600;
                request->send(200, "text/plain", "OK");
            } else {
                request->send(500, "text/plain", "Save Failed");
            }
            free_body_buffer(request);
            return;
        }

        if (url == "/api/schedule") {
            StaticJsonDocument<2048> doc;
            DeserializationError error = deserializeJson(doc, *body);
            if (error) {
                request->send(400, "text/plain", "Invalid JSON");
                free_body_buffer(request);
                return;
            }

            JsonArray schedule = doc["schedule"].as<JsonArray>();
            if (schedule.isNull() || schedule.size() != 7) {
                request->send(400, "text/plain", "Invalid schedule payload");
                free_body_buffer(request);
                return;
            }

            for (int day = 0; day < 7; day++) {
                uint64_t parsed_mask = 0;
                if (!parse_slot_mask(schedule[day], parsed_mask)) {
                    request->send(400, "text/plain", "Invalid schedule value");
                    free_body_buffer(request);
                    return;
                }
                configManager.data.weekSchedule[day].timeSlots = parsed_mask;
            }

            if (configManager.saveConfig()) {
                request->send(200, "text/plain", "Schedule Saved");
            } else {
                request->send(500, "text/plain", "Save Failed");
            }
            free_body_buffer(request);
            return;
        }
    });

    server.begin();
}

void web_interface_loop() {
    if (!restart_pending) return;
    if (millis() < restart_at_ms) return;
    restart_pending = false;
    ESP.restart();
}
