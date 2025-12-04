# 📋 REFACTOR COMPLETO: Sistema Prompt Dinamico

## 🔍 Analisi Flusso Attuale

### Flusso di Creazione/Visualizzazione/Salvataggio

```
┌─────────────────────────────────────────────────────────────┐
│                    FRONTEND (index.html)                    │
├─────────────────────────────────────────────────────────────┤
│ 1. CARICA JSON                                              │
│    GET /api/assistant/prompt                                │
│    → handleAssistantPromptGet()                             │
│    → Legge VOICE_ASSISTANT_PROMPT_JSON_PATH                 │
│    → Mostra nel textarea editor                             │
│                                                              │
│ 2. ANTEPRIMA (quando modifichi il JSON)                     │
│    POST /api/assistant/prompt/preview                       │
│    → handleAssistantPromptPreview()                         │
│    → executeAutoPopulateCommands() ← ESEGUE COMANDI         │
│    → buildPromptFromJson()                                  │
│       → composeSystemPrompt()                               │
│          → Sostituisce {{COMMAND_LIST}}                     │
│          → Sostituisce {{BLE_HOSTS}}                        │
│          → Aggiunge sections[]                              │
│          → resolvePromptVariables() ← SOSTITUISCE {{var}}   │
│    → Ritorna JSON con resolvedPrompt + variables            │
│                                                              │
│ 3. SALVA PROMPT                                             │
│    POST /api/assistant/prompt                               │
│    → handleAssistantPromptPost()                            │
│    → savePromptDefinition()                                 │
│    → Scrive il JSON grezzo su LittleFS                      │
│    → reloadPromptDefinition()                               │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│              USO IN RUNTIME (Voice Assistant)               │
├─────────────────────────────────────────────────────────────┤
│ Quando arriva una richiesta vocale:                         │
│                                                              │
│ 1. getSystemPrompt()                                        │
│    → Carica il JSON salvato                                 │
│    → composeSystemPrompt()                                  │
│       → NON esegue auto_populate (solo a preview!)          │
│       → Sostituisce {{COMMAND_LIST}}                        │
│       → Sostituisce {{BLE_HOSTS}}                           │
│       → Aggiunge sections[] con placeholder NON risolti     │
│       → resolvePromptVariables()                            │
│          → Sostituisce {{var}} con valori in memoria        │
│                                                              │
│ 2. Invia il prompt al LLM                                   │
│                                                              │
│ 3. Dopo l'esecuzione comando:                               │
│    → captureCommandOutputVariables()                        │
│       → Salva {{command_NAME_output}}                       │
│       → Salva {{command_NAME_refined_output}}               │
│       → Salva {{last_command_name}}                         │
└─────────────────────────────────────────────────────────────┘
```

### Struttura JSON Attuale

```json
{
  "prompt_template": "Testo con {{COMMAND_LIST}} e {{BLE_HOSTS}}",
  "auto_populate": [
    {
      "command": "lua_script",
      "args": ["codice lua"]
    }
  ],
  "sections": [
    "Sezione 1 con {{command_lua_script_output}}",
    "Sezione 2 con {{command_lua_script_refined_output}}"
  ]
}
```

---

## ❌ Problemi del Sistema Attuale

### 1. **Due Modalità di Risoluzione Confuse**
- **Preview**: esegue `auto_populate` e risolve variabili
- **Runtime**: NON esegue `auto_populate`, usa solo variabili in memoria

### 2. **L'utente vuole Pre-Risolvere il Prompt**
Tu hai detto:
> "voglio che si risolva ancora prima e salvarlo"

Vuoi:
1. Eseguire i comandi una volta
2. Prendere i risultati
3. **Incorporarli direttamente nel testo salvato**
4. Non risolverli ogni volta dinamicamente

### 3. **Complessità Inutile**
- Sistema `auto_populate` eseguito solo in preview
- Sistema di variabili dinamiche {{var}}
- Due meccanismi di sostituzione sovrapposti

---

## ✅ REFACTOR PROPOSTO

### Obiettivo
**Lasciare solo la funzione di risoluzione automatica, ma incorporare il risultato nel testo salvato**

### Nuovo Flusso

```
┌─────────────────────────────────────────────────────────────┐
│               FRONTEND con "Risolvi e Salva"                │
├─────────────────────────────────────────────────────────────┤
│ 1. CARICA JSON                                              │
│    GET /api/assistant/prompt                                │
│    → Mostra il JSON nel textarea                            │
│                                                              │
│ 2. ANTEPRIMA LIVE (opzionale, per vedere il risultato)      │
│    POST /api/assistant/prompt/preview                       │
│    → Esegue auto_populate                                   │
│    → Sostituisce placeholder                                │
│    → Mostra anteprima                                       │
│                                                              │
│ 3. [NUOVO] RISOLVI E SALVA                                  │
│    POST /api/assistant/prompt/resolve-and-save              │
│    → executeAutoPopulateCommands()                          │
│    → Ottiene tutti i valori risolti                         │
│    → Sostituisce {{var}} con valori REALI nel JSON          │
│    → Salva il JSON con valori incorporati                   │
│    → Ritorna il nuovo JSON salvato                          │
│                                                              │
│ 4. SALVA DIRETTO (senza risoluzione)                        │
│    POST /api/assistant/prompt                               │
│    → Salva JSON così com'è                                  │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│              USO IN RUNTIME (Semplificato)                  │
├─────────────────────────────────────────────────────────────┤
│ 1. getSystemPrompt()                                        │
│    → Legge il JSON                                          │
│    → Sostituisce solo {{COMMAND_LIST}} e {{BLE_HOSTS}}     │
│       (sempre dinamici)                                     │
│    → Aggiunge sections[] (già risolte)                      │
│    → Nessuna altra risoluzione dinamica                     │
│                                                              │
│ 2. Invia al LLM                                             │
└─────────────────────────────────────────────────────────────┘
```

### Nuovo JSON Salvato (dopo risoluzione)

**Prima (con placeholder):**
```json
{
  "prompt_template": "Sei un assistente. Comandi: {{COMMAND_LIST}}",
  "auto_populate": [...],
  "sections": [
    "Meteo: {{command_lua_script_refined_output}}"
  ]
}
```

**Dopo "Risolvi e Salva" (valori incorporati):**
```json
{
  "prompt_template": "Sei un assistente. Comandi: {{COMMAND_LIST}}",
  "sections": [
    "Meteo: A Milano ci sono 15°C, cielo nuvoloso."
  ]
}
```

---

## 🔧 Modifiche da Implementare

### 1. Backend - Nuovo Endpoint

#### File: `src/core/web_server_manager.cpp`

**Aggiungere:**
```cpp
// Nel setup:
server_->on("/api/assistant/prompt/resolve-and-save", HTTP_POST,
    [this]() { handleAssistantPromptResolveAndSave(); });

void WebServerManager::handleAssistantPromptResolveAndSave() {
    String body = server_->arg("plain");
    if (body.isEmpty()) {
        sendJson(400, "{\"status\":\"error\",\"message\":\"Empty body\"}");
        return;
    }

    VoiceAssistant& assistant = VoiceAssistant::getInstance();
    std::string error;
    std::string resolved_json;

    // Esegue auto_populate e risolve tutti i placeholder
    if (!assistant.resolveAndSavePrompt(std::string(body.c_str()),
                                        error,
                                        resolved_json)) {
        StaticJsonDocument<256> response;
        response["status"] = "error";
        response["message"] = error;
        String payload;
        serializeJson(response, payload);
        sendJson(400, payload);
        return;
    }

    // Ritorna il nuovo JSON risolto
    StaticJsonDocument<512> response;
    response["status"] = "success";
    response["message"] = "Prompt risolto e salvato";
    response["resolved_json"] = resolved_json.c_str();
    String payload;
    serializeJson(response, payload);
    sendJson(200, payload);
}
```

### 2. Backend - Nuova Funzione in VoiceAssistant

#### File: `src/core/voice_assistant.h`

**Aggiungere dichiarazione:**
```cpp
bool resolveAndSavePrompt(const std::string& raw_json,
                          std::string& error,
                          std::string& resolved_json_out);
```

#### File: `src/core/voice_assistant.cpp`

**Implementare:**
```cpp
bool VoiceAssistant::resolveAndSavePrompt(const std::string& raw_json,
                                          std::string& error,
                                          std::string& resolved_json_out) {
    // 1. Parse del JSON
    VoiceAssistantPromptDefinition definition;
    if (!parsePromptDefinition(raw_json, definition, error)) {
        return false;
    }

    // 2. Esegui auto_populate per popolare le variabili
    if (!executeAutoPopulateCommands(raw_json, error)) {
        LOG_W("Auto-populate failed: %s", error.c_str());
    }

    // 3. Risolvi le sections sostituendo i placeholder
    std::vector<std::string> resolved_sections;
    for (const auto& section : definition.sections) {
        std::string resolved = resolvePromptVariables(section);
        resolved_sections.push_back(resolved);
    }

    // 4. Crea nuovo JSON con sections risolte
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        error = "Failed to create JSON";
        return false;
    }

    // Mantieni prompt_template
    cJSON_AddStringToObject(root, "prompt_template",
                            definition.prompt_template.c_str());

    // Aggiungi sections risolte
    cJSON* sections_array = cJSON_CreateArray();
    for (const auto& section : resolved_sections) {
        cJSON_AddItemToArray(sections_array,
                             cJSON_CreateString(section.c_str()));
    }
    cJSON_AddItemToObject(root, "sections", sections_array);

    // NON includere auto_populate (non serve più)

    // 5. Serializza e salva
    char* json_string = cJSON_Print(root);
    if (!json_string) {
        cJSON_Delete(root);
        error = "Failed to serialize JSON";
        return false;
    }

    resolved_json_out = json_string;
    cJSON_free(json_string);
    cJSON_Delete(root);

    // 6. Salva su filesystem
    if (!savePromptDefinition(resolved_json_out, error)) {
        return false;
    }

    return true;
}
```

### 3. Frontend - Nuovo Pulsante

#### File: `data/www/index.html`

**Modificare la sezione dei pulsanti:**
```html
<div class="card-actions">
  <button id="reload-prompt-json" class="secondary">Ricarica JSON</button>
  <button id="resolve-and-save-prompt">Risolvi e Salva</button>
  <button id="save-prompt-json" class="secondary">Salva Grezzo</button>
</div>
```

**Aggiungere JavaScript:**
```javascript
const resolveAndSaveBtn = document.getElementById('resolve-and-save-prompt');

async function resolveAndSavePrompt() {
  resolveAndSaveBtn.disabled = true;
  setStatusAnimated('Risoluzione in corso', 'busy');

  try {
    const response = await fetchWithTimeout('/api/assistant/prompt/resolve-and-save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: systemPromptEditor.value
    }, 30000); // 30s timeout per esecuzione comandi

    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.message || 'Risoluzione fallita');
    }

    // Aggiorna l'editor con il JSON risolto
    try {
      const formatted = JSON.stringify(JSON.parse(data.resolved_json), null, 2);
      systemPromptEditor.value = formatted;
    } catch {
      systemPromptEditor.value = data.resolved_json;
    }

    clearStatusAnimation();
    setStatus('✓ Prompt risolto e salvato', 'idle');
    await loadPromptDefinition(false);
  } catch (err) {
    clearStatusAnimation();
    setStatus('✗ ' + err.message, 'error');
    setTimeout(() => setStatus('Pronto', 'idle'), 5000);
  } finally {
    resolveAndSaveBtn.disabled = false;
  }
}

resolveAndSaveBtn.addEventListener('click', resolveAndSavePrompt);
```

### 4. Semplificare Runtime

#### File: `src/core/voice_assistant.cpp`

**Modificare `composeSystemPrompt()`:**

```cpp
std::string VoiceAssistant::composeSystemPrompt(
    const std::string& override_template,
    const VoiceAssistantPromptDefinition& prompt_definition) const {

    std::string prompt_template = override_template.empty()
        ? prompt_definition.prompt_template
        : override_template;

    if (prompt_template.empty()) {
        prompt_template = VOICE_ASSISTANT_FALLBACK_PROMPT_TEMPLATE;
    }

    // Sostituisci solo i placeholder SEMPRE dinamici
    auto commands = CommandCenter::getInstance().listCommands();
    std::string command_list = buildCommandList(commands);

    std::string host_list = buildBleHostList();

    std::string prompt = prompt_template;
    replaceAll(prompt, VOICE_ASSISTANT_COMMAND_LIST_PLACEHOLDER, command_list);
    replaceAll(prompt, VOICE_ASSISTANT_BLE_HOSTS_PLACEHOLDER, host_list);

    // Aggiungi sections (già risolte se salvate con "Risolvi e Salva")
    for (const auto& section : prompt_definition.sections) {
        prompt += section;
    }

    // NON chiamare resolvePromptVariables() - le variabili sono già risolte
    return prompt;
}
```

---

## 📊 Vantaggi del Refactor

### ✅ Risolve il Tuo Problema
- **Risolvi una volta, salva il risultato**
- Il prompt salvato contiene i dati reali, non placeholder
- Nessuna risoluzione dinamica al runtime

### ✅ Semplifica il Codice
- Elimina la confusione tra preview/runtime
- Un solo flusso chiaro
- Meno codice da mantenere

### ✅ Performance Migliori
- Nessuna esecuzione di comandi Lua al runtime
- Nessuna risoluzione di variabili dinamiche
- Prompt già pronto all'uso

### ✅ Flessibilità
- Puoi ancora salvare JSON con placeholder (Salva Grezzo)
- Puoi pre-risolvere quando vuoi (Risolvi e Salva)
- Puoi vedere anteprima prima di salvare

---

## 🗑️ Cosa Rimuovere (Opzionale - Fase 2)

Se vuoi **eliminare completamente** il sistema dinamico:

1. **Rimuovere:**
   - `auto_populate` dal JSON (dopo averlo risolto)
   - `executeAutoPopulateCommands()` dal runtime
   - `prompt_variables_` map (se non serve più)
   - `captureCommandOutputVariables()` dal runtime
   - `resolvePromptVariables()` dal runtime

2. **Mantenere:**
   - `{{COMMAND_LIST}}` - sempre dinamico (comandi cambiano)
   - `{{BLE_HOSTS}}` - sempre dinamico (device cambiano)
   - Sistema di risoluzione per preview/salvataggio

---

## 🎯 Implementazione a Fasi

### Fase 1: Aggiungere il Nuovo Flusso (Senza Rompere Nulla)
1. ✅ Aggiungere endpoint `/resolve-and-save`
2. ✅ Implementare `resolveAndSavePrompt()`
3. ✅ Aggiungere pulsante "Risolvi e Salva" nel frontend
4. ✅ Testare il nuovo flusso

### Fase 2: Deprecare Vecchio Sistema (Se Funziona)
1. Rimuovere risoluzione dinamica da runtime
2. Semplificare `composeSystemPrompt()`
3. Rimuovere codice non più usato
4. Aggiornare documentazione

---

## 📝 Note Importanti

### Variabili Sempre Dinamiche
Queste **devono rimanere dinamiche** perché cambiano:
- `{{COMMAND_LIST}}` - i comandi possono essere aggiunti/rimossi
- `{{BLE_HOSTS}}` - i device BLE si connettono/disconnettono

### Variabili da Pre-Risolvere
Queste possono essere risolte e salvate:
- `{{command_lua_script_output}}` - dati meteo
- `{{command_lua_script_refined_output}}` - dati meteo raffinati
- Qualsiasi altro dato statico che vuoi incorporare

### Quando Usare "Risolvi e Salva"
- Quando hai dati che cambiano raramente (meteo, news, ecc.)
- Quando vuoi un prompt "snapshot" fisso
- Quando vuoi evitare esecuzioni ripetute di comandi costosi

### Quando Usare "Salva Grezzo"
- Quando vuoi mantenere i placeholder per risoluzione futura
- Quando stai ancora sviluppando il prompt
- Quando vuoi testare diverse configurazioni

---

## 🚀 Prossimi Passi

Vuoi che implementi:
1. **Solo il nuovo endpoint "Risolvi e Salva"** (Fase 1)?
2. **Refactor completo con rimozione sistema dinamico** (Fase 1+2)?
3. **Una soluzione diversa** basata su altre esigenze?

Fammi sapere come procedere!
