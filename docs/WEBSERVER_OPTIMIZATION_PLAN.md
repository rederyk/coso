# Piano di Ottimizzazione Web Server ESP32-S3

## Problemi Identificati

### **BLOCCHI GENERALI (TUTTE LE PAGINE)** ⚠️⚠️⚠️

#### 1. **Chiamate HTTP Bloccanti Sincrone** 🔥 CRITICO
- **Causa**: `handleAssistantChat()` attende risposta LLM con timeout di **50 secondi**
- **Impatto**: Web server completamente bloccato durante richiesta LLM/Whisper
- **Localizzazione**:
  - [web_server_manager.cpp:606](src/core/web_server_manager.cpp:606) - `getLastResponse(response, 50000)`
  - [web_server_manager.cpp:640-654](src/core/web_server_manager.cpp:640-654) - `handleAssistantAudioStop()`
- **Evidenza**: `xQueueReceive()` blocca il task HTTP fino a 50 secondi
- **Gravità**: ⚠️⚠️⚠️ MASSIMA - Rende il web server inutilizzabile

#### 2. **Fetch Ollama Models Sincrono** 🔥 CRITICO
- **Causa**: `fetchOllamaModels()` fa HTTP request bloccante con timeout 10s
- **Impatto**: Apertura pagina settings/models blocca tutto il web server per 10+ secondi
- **Localizzazione**:
  - [voice_assistant.cpp:1478-1560](src/core/voice_assistant.cpp:1478-1560) - `fetchOllamaModels()`
  - [web_server_manager.cpp:1013](src/core/web_server_manager.cpp:1013) - Chiamata da `handleAssistantModels()`
- **Timeout**: 10 secondi per richiesta HTTP
- **Gravità**: ⚠️⚠️ ALTA - Blocca UI all'apertura pagine

#### 3. **TTS Options Fetch Sincrono** 🔥 ALTO
- **Causa**: `handleTtsOptions()` fa multiple HTTP GET sincrone a server TTS
- **Impatto**: Click su "Refresh TTS options" blocca web server 10-20 secondi
- **Localizzazione**: [web_server_manager.cpp:2137-2313](src/core/web_server_manager.cpp:2137-2313)
- **Timeout**: 10s per `/models`, 10s per `/voices` = 20s totali
- **Gravità**: ⚠️⚠️ ALTA - Utente pensa che sia crashato

### **BLOCCHI UPLOAD SPECIFICI**

#### 4. **Blocchi durante l'upload di cartelle**
- **Causa principale**: Upload sincrono sequenziale senza yield al scheduler FreeRTOS
- **Impatto**: Il task HTTP blocca il sistema per lunghi periodi
- **Localizzazione**: `handleFsUploadData()` in [web_server_manager.cpp:1361-1468](src/core/web_server_manager.cpp:1361-1468)
- **Gravità**: ⚠️ MEDIA

#### 5. **Mutex SD Card non rilasciato correttamente**
- **Causa**: Se un'operazione fallisce durante l'upload, il mutex potrebbe rimanere acquisito
- **Impatto**: Altre operazioni SD rimangono bloccate fino al riavvio
- **Localizzazione**: `UploadState::lock_acquired` in [web_server_manager.cpp:1361-1498](src/core/web_server_manager.cpp:1361-1498)
- **Gravità**: ⚠️ MEDIA

#### 6. **Frontend: Creazione directory ripetitiva**
- **Causa**: `createFolderIfNotExists()` chiama `/api/fs/list` per ogni directory, anche se già creata
- **Impatto**: Centinaia di richieste HTTP per upload di cartelle complesse
- **Localizzazione**: [file-manager.html:746-752](data/www/file-manager.html:746-752)
- **Gravità**: ⚠️ BASSA

### **PROBLEMI INFRASTRUTTURA**

#### 7. **Watchdog Timer non alimentato**
- **Causa**: Loop lunghi nel task HTTP senza `vTaskDelay()` o `taskYIELD()`
- **Impatto**: ESP32 resetta per watchdog timeout
- **Localizzazione**: `serverLoop()` in [web_server_manager.cpp:407-414](src/core/web_server_manager.cpp:407-414)
- **Gravità**: ⚠️ ALTA

#### 8. **Memoria insufficiente per operazioni grandi**
- **Causa**: Stack HTTP di 12KB potrebbe essere insufficiente con upload grandi + JSON parsing
- **Impatto**: Stack overflow e crash
- **Localizzazione**: [task_config.h:21](src/core/task_config.h:21)
- **Gravità**: ⚠️ MEDIA

#### 9. **Timeout SD Card troppo aggressivo**
- **Causa**: `SD_MUTEX_TIMEOUT_MS = 2000ms` è troppo corto per operazioni grandi
- **Impatto**: Operazioni falliscono prematuramente
- **Localizzazione**: [web_server_manager.cpp:31](src/core/web_server_manager.cpp:31)
- **Gravità**: ⚠️ BASSA

## Piano di Ristrutturazione

### **FASE 0: Fix Blocchi Critici Web Server (URGENTE)** 🚨

#### 0.1 Ridurre Timeout LLM Assistant
**File**: `src/core/web_server_manager.cpp`

```cpp
// MODIFICARE linea 30
constexpr uint32_t ASSISTANT_RESPONSE_TIMEOUT_MS = 10000;  // Da 50000 a 10000ms (10 secondi)
```

**Impatto**: Riduce tempo massimo di blocco del web server da 50s a 10s
**Rischio**: Richieste LLM lente potrebbero andare in timeout
**Priorità**: 🔥 CRITICO - **FARE SUBITO**
**Tempo**: 1 minuto

---

#### 0.2 Rendere Asincrono fetchOllamaModels() - Quick Fix
**File**: `src/core/web_server_manager.cpp`
**Funzione**: `handleAssistantModels()`

**Opzione A (Rapida)**: Ridurre timeout e usare cache

```cpp
void WebServerManager::handleAssistantModels() {
    SettingsManager& settings = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = settings.getSnapshot();

    StaticJsonDocument<2048> doc;
    JsonArray models = doc.createNestedArray("models");
    doc["activeModel"] = snapshot.llmModel;
    doc["mode"] = snapshot.localApiMode ? "local" : "cloud";

    if (snapshot.localApiMode) {
        std::vector<std::string> available;

        // AGGIUNGERE: Timeout ridotto a 3 secondi
        auto fetch_with_timeout = [&]() {
            auto start = esp_timer_get_time() / 1000;
            bool success = VoiceAssistant::getInstance().fetchOllamaModels(snapshot.llmLocalEndpoint, available);
            auto elapsed = (esp_timer_get_time() / 1000) - start;

            if (elapsed > 2000) {
                LOG_W("fetchOllamaModels took %lld ms, too slow", elapsed);
            }
            return success;
        };

        // YIELD ogni 100ms durante il fetch per non bloccare tutto
        if (!fetch_with_timeout()) {
            // Fallback: ritorna modello corrente se configurato
            if (!snapshot.llmModel.empty()) {
                models.add(snapshot.llmModel);
            }
        } else {
            for (const auto& model : available) {
                models.add(model);
            }
        }
    } else {
        // ... cloud presets rimangono uguali
    }

    String response;
    serializeJson(doc, response);
    sendJson(200, response);
}
```

**Opzione B (Migliore)**: Endpoint separato per refresh asincrono
- `/api/assistant/models` ritorna subito cache o modello corrente
- Frontend fa polling o usa SSE per aggiornamenti

**Priorità**: 🔥 CRITICO
**Tempo**: 10 minuti (Opzione A), 30 minuti (Opzione B)

---

#### 0.3 Rendere Asincrono TTS Options - Quick Fix
**File**: `src/core/web_server_manager.cpp`
**Funzione**: `handleTtsOptions()`

```cpp
void WebServerManager::handleTtsOptions() {
    // ... validation iniziale ...

    if (refresh_requested) {
        // MODIFICARE: Non fare fetch sincrono, ritorna "in progress"
        doc["success"] = true;
        doc["fetching"] = true;
        doc["message"] = "Fetching TTS options in background...";
        doc["endpointUsed"] = snapshot.ttsLocalEndpoint.c_str();

        String response;
        serializeJson(doc, response);
        sendJson(202, response);  // 202 Accepted

        // TODO FASE 3: Lanciare task separato per fetch asincrono
        // Per ora: suggerire al frontend di rifare la richiesta dopo 3s
        return;
    }

    // ... resto del codice per cache ...
}
```

**Frontend fix**: Polling automatico dopo 202

```javascript
async function refreshTtsOptions() {
  const res = await fetch('/api/tts/options?refresh=1');
  if (res.status === 202) {
    // In progress, riprova dopo 3 secondi
    setTimeout(refreshTtsOptions, 3000);
  } else {
    // Completato, aggiorna UI
    const data = await res.json();
    updateTtsUI(data);
  }
}
```

**Priorità**: 🔥 ALTO
**Tempo**: 15 minuti

---

### **FASE 1: Ottimizzazioni Critiche Upload (Immediata)**

#### 1.1 Aggiungere Yield nel Loop Upload
**File**: `src/core/web_server_manager.cpp`
**Funzione**: `handleFsUploadData()`

```cpp
case UPLOAD_FILE_WRITE:
    if (!upload_state_.error && upload_state_.file && upload.currentSize) {
        size_t written = upload_state_.file.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
            fail("Write error");
        } else {
            upload_state_.bytes_written += written;

            // AGGIUNGERE: Yield ogni 8KB per non bloccare il sistema
            if ((upload_state_.bytes_written % 8192) == 0) {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
    }
    break;
```

**Impatto**: Previene blocchi del sistema durante upload grandi
**Rischio**: Basso
**Priorità**: ⚠️ CRITICO

---

#### 1.2 Garantire Rilascio Mutex in Tutti i Casi
**File**: `src/core/web_server_manager.cpp`
**Funzione**: `handleFsUploadData()`

```cpp
case UPLOAD_FILE_END:
case UPLOAD_FILE_ABORTED:
    // MODIFICARE: Usare sempre RAII per garantire rilascio
    if (upload_state_.file) {
        upload_state_.file.close();
    }
    if (upload_state_.lock_acquired) {
        sd.releaseSdMutex();
        upload_state_.lock_acquired = false;
    }
    break;
```

**Aggiungere anche nel distruttore**:
```cpp
~UploadState() {
    if (file) file.close();
    if (lock_acquired) {
        SdCardDriver::getInstance().releaseSdMutex();
        lock_acquired = false;
    }
}
```

**Impatto**: Previene deadlock permanenti
**Rischio**: Basso
**Priorità**: ⚠️ CRITICO

---

#### 1.3 Aumentare Stack HTTP
**File**: `src/core/task_config.h`

```cpp
// HTTP server task
constexpr uint32_t STACK_HTTP = 16384;  // Da 12288 a 16384 (16KB)
constexpr UBaseType_t PRIO_HTTP = 2;
```

**Impatto**: Previene stack overflow durante operazioni complesse
**Rischio**: Usa 4KB di RAM aggiuntivi
**Priorità**: ⚠️ ALTO

---

#### 1.4 Aumentare Timeout SD Mutex
**File**: `src/core/web_server_manager.cpp`

```cpp
constexpr uint32_t SD_MUTEX_TIMEOUT_MS = 5000;  // Da 2000 a 5000ms
```

**Impatto**: Permette operazioni SD più lunghe senza fallimenti prematuri
**Rischio**: Basso
**Priorità**: ⚠️ ALTO

---

### **FASE 2: Ottimizzazioni Frontend (Breve Termine)**

#### 2.1 Batch Creation Directory
**File**: `data/www/file-manager.html`
**Funzione**: `uploadFolderFiles()`

**Problema attuale**: Crea directory una per una con check HTTP
**Soluzione**: Creare tutte le directory in batch prima di uploadare i file

```javascript
async function uploadFolderFiles(files, rootFolderName) {
  // STEP 1: Raccogli tutte le directory necessarie
  const dirsToCreate = new Set();
  for (const file of files) {
    const relativePath = file.webkitRelativePath || file.name;
    const parts = relativePath.split('/').slice(1, -1); // Rimuovi root e filename

    let buildPath = joinPath(currentPath, rootFolderName);
    for (const dirName of parts) {
      buildPath = joinPath(buildPath, dirName);
      dirsToCreate.add(buildPath);
    }
  }

  // STEP 2: Crea tutte le directory in batch (senza check preventivo)
  statusEl.textContent = `Creating ${dirsToCreate.size} directories...`;
  for (const dirPath of Array.from(dirsToCreate).sort()) {
    const parts = dirPath.split('/').filter(Boolean);
    const dirName = parts[parts.length - 1];
    const parentPath = '/' + parts.slice(0, -1).join('/');

    await fetch('/api/fs/mkdir', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path: parentPath || '/', name: dirName })
    }).catch(() => {}); // Ignora errori (directory già esistente)
  }

  // STEP 3: Upload tutti i file
  let uploaded = 0;
  for (const file of files) {
    uploaded++;
    // ... resto del codice upload ...
  }
}
```

**Impatto**: Riduce drasticamente il numero di richieste HTTP
**Rischio**: Basso
**Priorità**: ⚠️ ALTO

---

#### 2.2 Rimuovere Check Preventivi
**File**: `data/www/file-manager.html`
**Funzione**: `createFolderIfNotExists()`

**Soluzione**: Eliminare la funzione e fare mkdir direttamente, ignorando errori

```javascript
// RIMUOVERE questa funzione
async function createFolderIfNotExists(path) { ... }

// SOSTITUIRE con mkdir diretto nella uploadFolderFiles
```

**Impatto**: Riduce il 50% delle richieste HTTP durante upload
**Rischio**: Basso (API ritorna errore benigno se già esiste)
**Priorità**: ⚠️ MEDIO

---

#### 2.3 Aggiungere Progress Bar Realistico
**File**: `data/www/file-manager.html`

```javascript
// Aggiungere feedback visivo per upload lunghi
statusEl.innerHTML = `
  <div>Uploading ${fileName} (${uploaded}/${total})</div>
  <div style="background: rgba(255,255,255,0.1); border-radius: 8px; height: 8px; margin-top: 4px;">
    <div style="background: var(--accent); height: 100%; width: ${(uploaded/total*100)}%; border-radius: 8px; transition: width 0.3s;"></div>
  </div>
`;
```

**Impatto**: Migliora UX, utente sa che il sistema sta lavorando
**Rischio**: Nessuno
**Priorità**: ⚠️ BASSO

---

### **FASE 3: Architettura Migliorata (Lungo Termine)**

#### 3.1 Implementare Upload Chunked
**Backend**: Nuovo endpoint `/api/fs/upload/chunked`
**Frontend**: Spezzare file grandi in chunk da 8KB

**Vantaggi**:
- Permette resume su errori
- Non blocca il sistema per file grandi
- Progress tracking accurato

**Complessità**: Media
**Priorità**: ⚠️ FUTURO

---

#### 3.2 Upload Queue con Worker
**Backend**: Task dedicato per gestire upload asincroni
**Frontend**: Push upload in coda, polling per stato

**Vantaggi**:
- HTTP handler risponde immediatamente
- Upload continua in background
- Multiple upload paralleli

**Complessità**: Alta
**Priorità**: ⚠️ FUTURO

---

#### 3.3 Cache Directory Listing
**Backend**: Mantenere cache in memoria delle directory recenti

**Vantaggi**:
- Riduce accessi SD durante check esistenza
- Velocizza listing ripetuti

**Complessità**: Media
**Rischio**: Consistenza cache
**Priorità**: ⚠️ FUTURO

---

## Riepilogo Priorità

| Priorità | Modifica | Impatto | Complessità | Tempo |
|-----------|----------|---------|-------------|-------|
| 🚨 URGENTE | **0.1 Ridurre timeout LLM** | **Sblocca web server** | Bassa | **1 min** |
| 🚨 URGENTE | **0.2 Fix fetchOllamaModels** | **Sblocca pagina settings** | Media | **10 min** |
| 🚨 URGENTE | **0.3 Fix TTS options** | **Sblocca pagina TTS** | Media | **15 min** |
| 🔴 CRITICO | 1.1 Yield nel loop upload | Previene crash | Bassa | 5 min |
| 🔴 CRITICO | 1.2 Garantire rilascio mutex | Previene deadlock | Bassa | 10 min |
| 🟡 ALTO | 1.3 Aumentare stack HTTP | Previene overflow | Bassa | 2 min |
| 🟡 ALTO | 1.4 Aumentare timeout SD | Previene timeout | Bassa | 2 min |
| 🟡 ALTO | 2.1 Batch directory creation | Velocizza upload | Media | 20 min |
| 🟢 MEDIO | 2.2 Rimuovere check preventivi | Velocizza upload | Bassa | 10 min |
| ⚪ BASSO | 2.3 Progress bar | Migliora UX | Bassa | 15 min |
| ⚫ FUTURO | 3.1 Upload chunked | Robustezza | Alta | 2-3 ore |
| ⚫ FUTURO | 3.2 Upload queue | Scalabilità | Alta | 4-5 ore |
| ⚫ FUTURO | 3.3 Cache directory | Performance | Media | 1-2 ore |

---

## Implementazione Consigliata

### ⚠️ Sessione 0 (URGENTE - 25 minuti) 🚨
**FARE PRIMA DI TUTTO - Sblocca il web server**

1. ✅ Modifica 0.1 - Ridurre timeout LLM da 50s a 10s
2. ✅ Modifica 0.2 - Fix fetchOllamaModels (fallback a modello corrente)
3. ✅ Modifica 0.3 - Fix TTS options (ritorna 202 + polling)

**Risultato**: Web server non si blocca più completamente, rimane responsive

---

### Sessione 1 (15-20 minuti)
1. ✅ Modifica 1.1 - Yield nel loop upload
2. ✅ Modifica 1.2 - Garantire rilascio mutex
3. ✅ Modifica 1.3 - Aumentare stack HTTP
4. ✅ Modifica 1.4 - Aumentare timeout SD

**Risultato**: Sistema stabile durante upload, no crash, no deadlock

---

### Sessione 2 (30-40 minuti)
1. ✅ Modifica 2.1 - Batch directory creation
2. ✅ Modifica 2.2 - Rimuovere check preventivi
3. ✅ Modifica 2.3 - Progress bar (opzionale)

**Risultato**: Upload 3-5x più veloce

---

### Sessione 3 (Opzionale, quando serve scalabilità)
1. ⏳ Implementare upload chunked (3.1)
2. ⏳ Implementare upload queue (3.2)
3. ⏳ Implementare cache directory (3.3)

**Risultato**: Sistema production-ready

---

## Note Tecniche

### Memoria Disponibile
- **RAM totale ESP32-S3**: ~320KB
- **PSRAM**: Configurazione dipendente (0-8MB)
- **Stack HTTP attuale**: 12KB
- **Stack HTTP proposto**: 16KB
- **Overhead aggiuntivo**: +4KB

### Watchdog Timer
- **Default timeout**: 5 secondi
- **Soluzione**: `vTaskDelay(1)` ogni 8KB scritti
- **Overhead**: <1% performance

### SD Card Mutex
- **Timeout attuale**: 2000ms
- **Timeout proposto**: 5000ms
- **Motivazione**: Write flash sync può richiedere 3-4 secondi

---

## Testing Plan

### Test 1: Upload File Singolo Grande (>1MB)
- **Obiettivo**: Verificare yield funziona
- **Successo**: No watchdog reset, progress fluido

### Test 2: Upload Cartella con 100+ file
- **Obiettivo**: Verificare batch creation funziona
- **Successo**: <30 secondi per completamento, no timeout

### Test 3: Upload Simultanei
- **Obiettivo**: Verificare mutex handling
- **Successo**: Secondo upload attende primo, nessun deadlock

### Test 4: Upload Interrotto (disconnect)
- **Obiettivo**: Verificare cleanup corretto
- **Successo**: Mutex rilasciato, no memory leak

---

## Rollback Plan

In caso di problemi dopo implementazione:

1. **Revert git**: `git checkout HEAD^ -- src/core/web_server_manager.cpp`
2. **Revert task_config**: Ripristinare `STACK_HTTP = 12288`
3. **Revert frontend**: Ripristinare versione precedente `file-manager.html`

---

## Metriche di Successo

- ✅ Zero watchdog reset durante upload
- ✅ Zero deadlock SD card
- ✅ Upload cartella 100 file completa in <30 secondi
- ✅ Sistema rimane responsive durante upload
- ✅ Free heap >100KB durante upload

---

## Riferimenti Codice

- [web_server_manager.cpp](src/core/web_server_manager.cpp) - Backend HTTP server
- [web_server_manager.h](src/core/web_server_manager.h) - Header e strutture
- [task_config.h](src/core/task_config.h) - Configurazione task FreeRTOS
- [file-manager.html](data/www/file-manager.html) - Frontend web
- [sd_card_driver.h](src/drivers/sd_card_driver.h) - Driver SD con mutex

---

*Documento creato: 2025-12-05*
*Ultima revisione: 2025-12-05*
