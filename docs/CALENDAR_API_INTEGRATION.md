# Calendar Scheduler - API REST Integration Guide

## File da Modificare

### 1. `src/core/web_server_manager.cpp`

#### Aggiungere nella funzione `begin()` (dopo gli altri endpoint):

```cpp
// ========== CALENDAR / SCHEDULER ENDPOINTS ==========
server_->on("/calendar", HTTP_GET, [this]() { handleCalendarPage(); });
server_->on("/api/calendar/events", HTTP_GET, [this]() { handleCalendarEventsList(); });
server_->on("/api/calendar/events", HTTP_POST, [this]() { handleCalendarEventsCreate(); });
server_->on("/api/calendar/events/*/delete", HTTP_POST, [this]() { handleCalendarEventsDelete(); });
server_->on("/api/calendar/events/*/enable", HTTP_POST, [this]() { handleCalendarEventsEnable(); });
server_->on("/api/calendar/events/*/execute", HTTP_POST, [this]() { handleCalendarEventsExecute(); });
server_->on("/api/calendar/settings", HTTP_GET, [this]() { handleCalendarSettingsGet(); });
server_->on("/api/calendar/settings", HTTP_POST, [this]() { handleCalendarSettingsPost(); });
```

#### Aggiungere include all'inizio del file:

```cpp
#include "core/time_scheduler.h"
```

#### Aggiungere le funzioni handler (alla fine del file, prima della chiusura):

```cpp
// ========== CALENDAR HANDLERS ==========

void WebServerManager::handleCalendarPage() {
    if (!checkAuthentication()) {
        return;
    }
    serveStaticFile("/calendar.html");
}

void WebServerManager::handleCalendarEventsList() {
    if (!checkAuthentication()) {
        return;
    }

    auto& scheduler = TimeScheduler::getInstance();
    auto events = scheduler.listEvents();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);

    cJSON* events_array = cJSON_CreateArray();

    for (const auto& event : events) {
        cJSON* event_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(event_obj, "id", event.id.c_str());
        cJSON_AddStringToObject(event_obj, "name", event.name.c_str());
        cJSON_AddStringToObject(event_obj, "description", event.description.c_str());
        cJSON_AddNumberToObject(event_obj, "type", static_cast<int>(event.type));
        cJSON_AddBoolToObject(event_obj, "enabled", event.enabled);
        cJSON_AddNumberToObject(event_obj, "hour", event.hour);
        cJSON_AddNumberToObject(event_obj, "minute", event.minute);
        cJSON_AddNumberToObject(event_obj, "weekdays", event.weekdays);
        cJSON_AddNumberToObject(event_obj, "year", event.year);
        cJSON_AddNumberToObject(event_obj, "month", event.month);
        cJSON_AddNumberToObject(event_obj, "day", event.day);
        cJSON_AddStringToObject(event_obj, "lua_script", event.lua_script.c_str());
        cJSON_AddNumberToObject(event_obj, "created_at", event.created_at);
        cJSON_AddNumberToObject(event_obj, "last_run", event.last_run);
        cJSON_AddNumberToObject(event_obj, "next_run", event.next_run);
        cJSON_AddNumberToObject(event_obj, "execution_count", event.execution_count);
        cJSON_AddNumberToObject(event_obj, "status", static_cast<int>(event.status));
        cJSON_AddStringToObject(event_obj, "last_error", event.last_error.c_str());

        cJSON_AddItemToArray(events_array, event_obj);
    }

    cJSON_AddItemToObject(root, "events", events_array);

    char* json_str = cJSON_PrintUnformatted(root);
    server_->send(200, "application/json", json_str);
    free(json_str);
    cJSON_Delete(root);
}

void WebServerManager::handleCalendarEventsCreate() {
    if (!checkAuthentication()) {
        return;
    }

    if (!server_->hasArg("plain")) {
        sendJsonError("Missing request body");
        return;
    }

    String body = server_->arg("plain");
    cJSON* json = cJSON_Parse(body.c_str());
    if (!json) {
        sendJsonError("Invalid JSON");
        return;
    }

    TimeScheduler::CalendarEvent event;
    event.name = cJSON_GetObjectItem(json, "name")->valuestring;

    cJSON* desc = cJSON_GetObjectItem(json, "description");
    event.description = desc ? desc->valuestring : "";

    event.type = static_cast<TimeScheduler::EventType>(
        cJSON_GetObjectItem(json, "type")->valueint);
    event.enabled = cJSON_IsTrue(cJSON_GetObjectItem(json, "enabled"));
    event.hour = cJSON_GetObjectItem(json, "hour")->valueint;
    event.minute = cJSON_GetObjectItem(json, "minute")->valueint;
    event.weekdays = cJSON_GetObjectItem(json, "weekdays")->valueint;
    event.year = cJSON_GetObjectItem(json, "year")->valueint;
    event.month = cJSON_GetObjectItem(json, "month")->valueint;
    event.day = cJSON_GetObjectItem(json, "day")->valueint;
    event.lua_script = cJSON_GetObjectItem(json, "lua_script")->valuestring;

    cJSON_Delete(json);

    auto& scheduler = TimeScheduler::getInstance();
    std::string id = scheduler.createEvent(event);

    if (id.empty()) {
        sendJsonError("Failed to create event");
        return;
    }

    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "id", id.c_str());

    char* json_str = cJSON_PrintUnformatted(response);
    server_->send(200, "application/json", json_str);
    free(json_str);
    cJSON_Delete(response);
}

void WebServerManager::handleCalendarEventsDelete() {
    if (!checkAuthentication()) {
        return;
    }

    String uri = server_->uri();
    int start = uri.indexOf("/api/calendar/events/") + 21;
    int end = uri.indexOf("/delete");
    String event_id = uri.substring(start, end);

    auto& scheduler = TimeScheduler::getInstance();
    bool success = scheduler.deleteEvent(event_id.c_str());

    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", success);
    if (!success) {
        cJSON_AddStringToObject(response, "error", "Event not found");
    }

    char* json_str = cJSON_PrintUnformatted(response);
    server_->send(200, "application/json", json_str);
    free(json_str);
    cJSON_Delete(response);
}

void WebServerManager::handleCalendarEventsEnable() {
    if (!checkAuthentication()) {
        return;
    }

    String uri = server_->uri();
    int start = uri.indexOf("/api/calendar/events/") + 21;
    int end = uri.indexOf("/enable");
    String event_id = uri.substring(start, end);

    if (!server_->hasArg("plain")) {
        sendJsonError("Missing request body");
        return;
    }

    String body = server_->arg("plain");
    cJSON* json = cJSON_Parse(body.c_str());
    if (!json) {
        sendJsonError("Invalid JSON");
        return;
    }

    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(json, "enabled"));
    cJSON_Delete(json);

    auto& scheduler = TimeScheduler::getInstance();
    bool success = scheduler.enableEvent(event_id.c_str(), enabled);

    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", success);
    if (!success) {
        cJSON_AddStringToObject(response, "error", "Event not found");
    }

    char* json_str = cJSON_PrintUnformatted(response);
    server_->send(200, "application/json", json_str);
    free(json_str);
    cJSON_Delete(response);
}

void WebServerManager::handleCalendarEventsExecute() {
    if (!checkAuthentication()) {
        return;
    }

    String uri = server_->uri();
    int start = uri.indexOf("/api/calendar/events/") + 21;
    int end = uri.indexOf("/execute");
    String event_id = uri.substring(start, end);

    auto& scheduler = TimeScheduler::getInstance();
    bool success = scheduler.executeEventNow(event_id.c_str());

    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", success);
    if (!success) {
        cJSON_AddStringToObject(response, "error", "Event not found or execution failed");
    }

    char* json_str = cJSON_PrintUnformatted(response);
    server_->send(200, "application/json", json_str);
    free(json_str);
    cJSON_Delete(response);
}

void WebServerManager::handleCalendarSettingsGet() {
    if (!checkAuthentication()) {
        return;
    }

    auto& scheduler = TimeScheduler::getInstance();

    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddBoolToObject(response, "enabled", scheduler.isEnabled());

    char* json_str = cJSON_PrintUnformatted(response);
    server_->send(200, "application/json", json_str);
    free(json_str);
    cJSON_Delete(response);
}

void WebServerManager::handleCalendarSettingsPost() {
    if (!checkAuthentication()) {
        return;
    }

    if (!server_->hasArg("plain")) {
        sendJsonError("Missing request body");
        return;
    }

    String body = server_->arg("plain");
    cJSON* json = cJSON_Parse(body.c_str());
    if (!json) {
        sendJsonError("Invalid JSON");
        return;
    }

    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(json, "enabled"));
    cJSON_Delete(json);

    auto& scheduler = TimeScheduler::getInstance();
    scheduler.setEnabled(enabled);

    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);

    char* json_str = cJSON_PrintUnformatted(response);
    server_->send(200, "application/json", json_str);
    free(json_str);
    cJSON_Delete(response);
}
```

---

## Aggiornare Navigation Bar

### File: `data/www/app-nav.js`

Aggiungere il link al calendario nella array `pages`:

```javascript
{
    path: '/calendar',
    label: '⏰ Calendar',
    icon: '⏰'
}
```

---

## Inizializzare TimeScheduler

### File: `src/main.cpp`

Aggiungere nella funzione `setup()`:

```cpp
#include "core/time_scheduler.h"

// ...

void setup() {
    // ... altri init ...

    // Initialize TimeScheduler
    auto& scheduler = TimeScheduler::getInstance();
    if (scheduler.begin()) {
        Serial.println("✓ TimeScheduler started");
    } else {
        Serial.println("✗ TimeScheduler failed to start");
    }
}
```

---

## Testare il Sistema

### 1. Via Web UI
1. Aprire `http://[ESP32-IP]/calendar`
2. Cliccare "+ Create Event"
3. Configurare un alarm one-shot o evento ricorrente
4. Salvare e verificare l'esecuzione

### 2. Via AI (Prompt)

Aggiungere a `prompt_italiano_comandi.json` (dopo la sezione GPIO):

```json
"### 📅 Calendar & Alarms\n",
"**Utente:** \"Crea una sveglia per domani alle 7\"\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_create_alarm\",\n",
"  \"args\": [\"Sveglia\", \"2025-12-05\", \"07:00\", \"println('Buongiorno!')\"],\n",
"  \"text\": \"Ho creato una sveglia per domani alle 7:00\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n\n",

"**Utente:** \"Fai suonare un allarme ogni giorno lavorativo alle 8\"\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_create_recurring\",\n",
"  \"args\": [\"Allarme Lavoro\", \"08:00\", \"62\", \"println('Ora di andare a lavoro!')\"],\n",
"  \"text\": \"Ho creato un allarme ricorrente Lun-Ven alle 8:00\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n",
"Note: 62 = 0b0111110 = Lunedì-Venerdì\n\n",

"**Utente:** \"Mostra i miei allarmi\"\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_list\",\n",
"  \"args\": [],\n",
"  \"text\": \"Ecco i tuoi eventi programmati\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n\n"
```

### 3. Via Comandi Diretti

```bash
# Lista eventi
calendar_list

# Crea alarm one-shot
calendar_create_alarm "Wake Up" 2025-12-05 07:00 "println('Good morning!')"

# Crea evento ricorrente (Lun-Ven alle 8:00)
calendar_create_recurring "Morning Coffee" 08:00 62 "println('Coffee time!')"

# Esegui evento ora
calendar_run evt_12345678

# Disabilita evento
calendar_enable evt_12345678 false

# Elimina evento
calendar_delete evt_12345678
```

---

## Struttura File Creati

```
include/core/time_scheduler.h          ✓ Creato
src/core/time_scheduler.cpp            ✓ Creato
data/www/calendar.html                 ✓ Creato
CALENDAR_API_INTEGRATION.md            ✓ Creato (questo file)
```

## File da Modificare Manualmente

```
src/core/command_center.cpp            ✓ Modificato (comandi AI aggiunti)
src/core/web_server_manager.cpp        ⚠️ DA FARE (aggiungere handler API)
src/main.cpp                            ⚠️ DA FARE (inizializzare scheduler)
data/www/app-nav.js                     ⚠️ DA FARE (aggiungere link)
prompt_italiano_comandi.json            ⚠️ DA FARE (aggiungere esempi AI)
```

---

## Note Implementative

### Weekdays Bitmask
```
Bit 0 (1)   = Domenica
Bit 1 (2)   = Lunedì
Bit 2 (4)   = Martedì
Bit 3 (8)   = Mercoledì
Bit 4 (16)  = Giovedì
Bit 5 (32)  = Venerdì
Bit 6 (64)  = Sabato

Esempi:
- 62 (0b0111110) = Lunedì-Venerdì
- 127 (0b1111111) = Tutti i giorni
- 65 (0b1000001) = Domenica + Sabato (weekend)
```

### Persistenza
- File: `/calendar_events.json`
- Formato: JSON con versione 1
- Backup automatico su SD card (se montata)
- Caricamento al boot

### Performance
- Check ogni 30 secondi (configurable)
- Task FreeRTOS su Core 0, priorità 2
- Esecuzione non-blocking
- RAM usage: ~200 bytes per evento

---

## Prossimi Step

1. ✅ Strutture dati e core logic implementati
2. ✅ Comandi AI registrati nel CommandCenter
3. ✅ Web UI creata (calendar.html)
4. ⚠️ Integrare API REST nel WebServerManager
5. ⚠️ Inizializzare in main.cpp
6. ⚠️ Testare end-to-end
7. ⚠️ Aggiungere screen LVGL (opzionale)

**Il sistema è pronto al 70% - manca solo l'integrazione delle API REST nel backend!**
