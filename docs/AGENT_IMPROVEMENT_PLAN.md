# 🤖 Piano Miglioramento Agente ESP32 Voice Assistant

## 📋 Analisi Situazione Attuale

### Architettura Esistente
L'agente vocale ESP32 è composto da:
- **VoiceAssistant**: 3 task FreeRTOS (recording, STT, AI processing)
- **CommandCenter**: Sistema registro/esecuzione comandi
- **LuaSandbox**: Interprete Lua per script complessi
- **ConversationBuffer**: Persistenza conversazioni con limite 30 entry
- **WebDataManager**: Download/caching dati web con scheduling

### Flusso Attuale
```
User Speech → Recording → Whisper STT → GPT LLM →
→ Command/Script Parsing → Execution → Output → Response
```

### Problema Identificato
**Gestione Output Non Ottimale**:
- L'output dei comandi viene mostrato direttamente all'utente (es. JSON raw da API meteo)
- Non c'è opportunità di riformattare o riassumere output complessi
- La conversazione viene "inquinata" con dati tecnici invece di risposte user-friendly

### Esempio Problematico
```
User: "Che tempo fa a Piombino?"
Agent: lua_script → webData.fetch_once(...) → "{"latitude":43.5625,...}"
Output Grezzo: Mostra tutto il JSON all'utente ❌
```

## 🎯 Obiettivi Miglioramento

### Obiettivo 1: Output Refinement Pipeline
**Problema**: Output grezzi (JSON, log tecnici) mostrati direttamente all'utente
**Soluzione**: Pipeline di post-elaborazione output prima di mostrarlo

### Obiettivo 2: Multi-Instance Context Sharing
**Problema**: Un'istanza chat non può processare output di un'altra
**Soluzione**: Sistema di "handoff" tra istanze LLM con contesto

### Obiettivo 3: Intelligent Output Routing
**Problema**: Decisione hardcoded su quando processare output
**Soluzione**: LLM decide autonomamente se serve post-processing

## 🏗️ Roadmap Implementazione

---

## **FASE 1: Output Refinement System** (Priorità: ALTA)
**Durata stimata**: 1-2 sessioni
**Dipendenze**: Nessuna

### Task 1.1: Estendere VoiceCommand Structure
**File**: `src/core/voice_assistant.h`

Aggiungere campi per gestire output processing:
```cpp
struct VoiceCommand {
    std::string command;
    std::vector<std::string> args;
    std::string text;           // LLM response text
    std::string transcription;  // User speech
    std::string output;         // Command raw output

    // NUOVO:
    std::string refined_output; // Post-processed output
    bool needs_refinement;      // Flag se serve post-processing
    std::string refinement_prompt; // Prompt per raffinamento
};
```

**Rationale**: Separare output grezzo da output raffinato mantiene trasparenza

---

### Task 1.2: Implementare Output Refinement Function
**File**: `src/core/voice_assistant.cpp`

Aggiungere metodo per raffinare output:
```cpp
bool VoiceAssistant::refineCommandOutput(VoiceCommand& cmd) {
    if (cmd.output.empty() || !cmd.needs_refinement) {
        return false;
    }

    // Costruisci prompt di raffinamento
    std::string refinement_prompt = buildRefinementPrompt(cmd);

    // Chiama GPT per raffinare
    std::string refined;
    bool success = makeGPTRequest(refinement_prompt, refined);

    if (success) {
        cmd.refined_output = refined;
        LOG_I("Output refined: %s", refined.c_str());
        return true;
    }
    return false;
}

std::string VoiceAssistant::buildRefinementPrompt(const VoiceCommand& cmd) {
    std::string prompt = "L'utente ha chiesto: \"" + cmd.transcription + "\"\n";
    prompt += "Il comando ha prodotto questo output tecnico:\n";
    prompt += cmd.output + "\n\n";
    prompt += "Riassumi l'output in modo comprensibile e user-friendly per l'utente.\n";
    prompt += "Se sono dati meteo, formatta temperatura e condizioni.\n";
    prompt += "Rispondi solo con il testo formattato, niente altro.";
    return prompt;
}
```

**Benefici**:
- Output user-friendly automatico
- Conversazione pulita e comprensibile
- Flessibilità: LLM capisce contesto e formatta appropriatamente

---

### Task 1.3: Integrare Refinement nel Pipeline
**File**: `src/core/voice_assistant.cpp` - `aiProcessingTask()`

Modificare il task AI per chiamare refinement quando necessario:

```cpp
// DOPO l'esecuzione del comando (linea ~467)
if (!cmd.output.empty()) {
    LOG_I("Command output: %s", cmd.output.c_str());

    // NUOVO: Determina se serve raffinamento
    cmd.needs_refinement = shouldRefineOutput(cmd);

    if (cmd.needs_refinement) {
        LOG_I("Output needs refinement, processing...");
        bool refined = refineCommandOutput(cmd);

        if (refined && !cmd.refined_output.empty()) {
            // Usa output raffinato nella risposta
            cmd.text = cmd.refined_output;
        }
    }
}
```

Helper function:
```cpp
bool VoiceAssistant::shouldRefineOutput(const VoiceCommand& cmd) {
    // Criteri per determinare se serve refinement:

    // 1. Output molto lungo (>200 caratteri)
    if (cmd.output.size() > 200) return true;

    // 2. Output contiene JSON
    if (cmd.output.find('{') != std::string::npos) return true;

    // 3. Comandi specifici che producono dati grezzi
    if (cmd.command == "lua_script" &&
        cmd.output.find("webData") != std::string::npos) return true;

    return false;
}
```

**Risultato**:
- Esempio meteo diventa: "A Piombino ci sono 12.4°C con cielo nuvoloso"
- Invece di: `{"latitude":43.5625,"longitude":10.1875,...}`

---

## **FASE 2: Smart Output Decision** (Priorità: MEDIA)
**Durata stimata**: 1 sessione
**Dipendenze**: Fase 1 completata

### Task 2.1: LLM-Driven Refinement Decision
**File**: `src/core/voice_assistant.cpp`

Invece di euristica hardcoded, far decidere all'LLM se serve raffinamento:

```cpp
struct GPTCommandResponse {
    std::string command;
    std::vector<std::string> args;
    std::string text;
    bool should_refine_output; // NUOVO flag dal LLM
};

bool VoiceAssistant::parseGPTCommand(const std::string& response,
                                     VoiceCommand& cmd) {
    // Parsing JSON come prima...

    // NUOVO: Leggi campo should_refine_output
    cJSON* refine_flag = cJSON_GetObjectItem(root, "should_refine_output");
    if (refine_flag && cJSON_IsBool(refine_flag)) {
        cmd.needs_refinement = cJSON_IsTrue(refine_flag);
    } else {
        cmd.needs_refinement = false; // Default
    }

    return true;
}
```

Aggiornare system prompt:
```cpp
std::string VoiceAssistant::getSystemPrompt() const {
    // ... existing prompt ...

    prompt += "\n\nNUOVO FORMATO RISPOSTA JSON:\n";
    prompt += "{\n";
    prompt += "  \"command\": \"comando_da_eseguire\",\n";
    prompt += "  \"args\": [\"arg1\", \"arg2\"],\n";
    prompt += "  \"text\": \"Risposta testuale\",\n";
    prompt += "  \"should_refine_output\": true/false\n";
    prompt += "}\n\n";
    prompt += "Imposta should_refine_output=true se il comando produrrà ";
    prompt += "output tecnico (JSON, log) che deve essere riformattato.\n";
    prompt += "Esempio: webData.fetch_once → should_refine_output=true\n";

    return prompt;
}
```

**Vantaggio**: LLM sa meglio di noi quando serve raffinamento!

---

## **FASE 3: Multi-Instance Context Sharing** (Priorità: BASSA)
**Durata stimata**: 2 sessioni
**Dipendenze**: Fase 1 completata

### Scenario d'Uso
```
User: "Che tempo fa?"
Agent (Istanza 1): Esegue webData.fetch → JSON grezzo
Agent (Istanza 2): Riceve JSON + contesto → "Ci sono 12°C"
```

### Task 3.1: Output Forwarding Queue
**File**: `src/core/voice_assistant.h`

```cpp
class VoiceAssistant {
    // ... existing ...

    struct OutputForwardingRequest {
        std::string original_transcription;
        std::string command_executed;
        std::string raw_output;
        uint32_t timestamp;
    };

    QueueHandle_t outputForwardingQueue_;

    bool forwardOutputToSecondaryInstance(const VoiceCommand& cmd);
    void processForwardedOutput();
};
```

### Task 3.2: Secondary Processing Task
**File**: `src/core/voice_assistant.cpp`

```cpp
void VoiceAssistant::outputRefinementTask(void* param) {
    VoiceAssistant* va = static_cast<VoiceAssistant*>(param);

    while (va->initialized_) {
        OutputForwardingRequest* req = nullptr;
        if (xQueueReceive(va->outputForwardingQueue_, &req,
                          pdMS_TO_TICKS(1000)) == pdPASS) {

            // Costruisci prompt per "istanza 2"
            std::string prompt = "L'utente ha chiesto: \"" +
                               req->original_transcription + "\"\n";
            prompt += "Il sistema ha eseguito: " + req->command_executed + "\n";
            prompt += "Output ottenuto:\n" + req->raw_output + "\n\n";
            prompt += "Formatta questa informazione in modo user-friendly.";

            std::string refined;
            if (va->makeGPTRequest(prompt, refined)) {
                // Invia risposta raffinata a voiceCommandQueue
                VoiceCommand refined_cmd;
                refined_cmd.text = refined;
                refined_cmd.command = "output_refinement";
                // ... invia a queue ...
            }

            delete req;
        }
    }
}
```

**Trade-off**:
- ✅ Pro: Separazione netta elaborazione/presentazione
- ❌ Contro: Doppio costo API, latenza maggiore
- 🤔 Valutazione: Implementare solo se Fase 1 non basta

---

## **FASE 4: Persistent Context Enhancement** (Priorità: MEDIA)
**Durata stimata**: 1 sessione
**Dipendenze**: Nessuna (parallelo a Fase 1)

### Task 4.1: Estendere ConversationBuffer con Output Storage
**File**: `src/core/conversation_buffer.h`

```cpp
struct ConversationEntry {
    std::string role;
    std::string text;
    std::string output;         // Già presente
    std::string refined_output; // NUOVO
    uint32_t timestamp;
    std::string command;
    std::vector<std::string> args;
    std::string transcription;
};
```

Aggiornare serializzazione JSON per includere `refined_output`.

**Beneficio**:
- Storico completo conversazione (raw + refined)
- Debug facilitato
- Possibilità di "replay" conversazioni

---

### Task 4.2: Smart Context Window per LLM
**File**: `src/core/voice_assistant.cpp`

```cpp
std::string VoiceAssistant::buildConversationContext(
    size_t max_entries = 5) const {

    auto entries = ConversationBuffer::getInstance().getEntries();

    std::string context;
    size_t count = 0;

    for (auto it = entries.rbegin();
         it != entries.rend() && count < max_entries;
         ++it, ++count) {

        if (it->role == "user") {
            context = "User: " + it->transcription + "\n" + context;
        } else {
            // Usa refined_output se disponibile, altrimenti text
            std::string response = !it->refined_output.empty()
                                  ? it->refined_output
                                  : it->text;
            context = "Assistant: " + response + "\n" + context;
        }
    }

    return context;
}
```

Integrare nel prompt GPT:
```cpp
bool VoiceAssistant::makeGPTRequest(const std::string& user_input,
                                    std::string& response) {
    // ... setup ...

    // NUOVO: Includi contesto conversazione
    std::string context = buildConversationContext(5);
    std::string full_prompt = context + "\nUser: " + user_input;

    // ... resto richiesta GPT ...
}
```

**Beneficio**:
- LLM mantiene contesto conversazione
- Riferimenti anaforici funzionano ("Sì, mostrami quello")
- Conversazioni multi-turn più naturali

---

## **FASE 5: Testing & Validation** (Priorità: ALTA)
**Durata stimata**: 1 sessione
**Dipendenze**: Fase 1 completata

### Test Case 1: Meteo Query
```
Input: "Che tempo fa a Piombino?"
Expected:
  - Command: lua_script
  - Raw Output: {"latitude":43.5625,...}
  - Refined Output: "A Piombino ci sono 12.4°C, cielo nuvoloso"
  - User Sees: Refined Output ✓
```

### Test Case 2: Simple Command
```
Input: "Aumenta il volume"
Expected:
  - Command: volume_up
  - Raw Output: "Volume set to 70%"
  - Refined Output: (nessuno, non serve)
  - User Sees: Raw Output ✓
```

### Test Case 3: Multi-Turn Context
```
Turn 1:
  Input: "Che tempo fa?"
  Output: "Ci sono 12°C"

Turn 2:
  Input: "E domani?"
  Expected: LLM capisce contesto meteo e risponde appropriatamente
```

### Test Metrics
- ✅ Latenza refinement < 2 secondi
- ✅ Output user-friendly in 90%+ casi
- ✅ No inquinamento conversazione con dati tecnici
- ✅ Context window funziona per 5+ turni

---

## 📊 Roadmap Visuale

```
FASE 1: Output Refinement System [PRIORITÀ ALTA]
├── Task 1.1: Estendi VoiceCommand structure (30min)
├── Task 1.2: Implementa refineCommandOutput() (1h)
└── Task 1.3: Integra nel pipeline AI (1h)
    └── Test: Meteo query mostra output formattato ✓

FASE 2: Smart Output Decision [PRIORITÀ MEDIA]
└── Task 2.1: LLM-driven refinement flag (1h)
    └── Test: LLM decide correttamente quando raffinare ✓

FASE 3: Multi-Instance Context Sharing [PRIORITÀ BASSA]
├── Task 3.1: Forwarding queue (1.5h)
└── Task 3.2: Secondary processing task (2h)
    └── Test: Output forwarding funziona ✓
    └── Decision: Implementare solo se Fase 1 insufficiente

FASE 4: Persistent Context Enhancement [PRIORITÀ MEDIA]
├── Task 4.1: Estendi ConversationBuffer (30min)
└── Task 4.2: Smart context window (1h)
    └── Test: Multi-turn conversations funzionano ✓

FASE 5: Testing & Validation [PRIORITÀ ALTA]
└── Full integration test suite (2h)
```

---

## 🎯 Raccomandazione Iniziale

**IMPLEMENTARE SUBITO: FASE 1** (Output Refinement System)

### Perché?
1. ✅ **Impatto Immediato**: Risolve il problema principale (output grezzi)
2. ✅ **Basso Rischio**: Non cambia architettura, solo aggiunge post-processing
3. ✅ **Costo Ragionevole**: 1 chiamata GPT extra solo quando serve
4. ✅ **Facile Rollback**: Flag `needs_refinement` disabilitabile

### Stima Implementazione Fase 1
- **Tempo**: 2-3 ore sviluppo + 1 ora testing
- **Costo API**: ~0.002$ per refinement (GPT-3.5-turbo)
- **Latenza**: +1-2 secondi quando serve refinement
- **Memoria**: +~100 bytes per VoiceCommand

### Next Steps
1. Implementa Fase 1 (Task 1.1 → 1.2 → 1.3)
2. Testa con query meteo reali
3. Valuta se servono Fase 2-4 o se Fase 1 è sufficiente
4. Monitora latenza e costi API

---

## 🔧 Modifiche Sistemiche Necessarie

### File da Modificare
1. `src/core/voice_assistant.h` - VoiceCommand struct
2. `src/core/voice_assistant.cpp` - aiProcessingTask + nuovi metodi
3. `src/core/conversation_buffer.h` - ConversationEntry (Fase 4)
4. `src/core/voice_assistant.cpp` - getSystemPrompt() (Fase 2)

### Nessuna Modifica Necessaria
- ❌ CommandCenter (resta invariato)
- ❌ LuaSandbox (resta invariato)
- ❌ WebDataManager (resta invariato)
- ❌ UI/Screens (output già gestito via voiceCommandQueue)

---

## 💡 Alternative Considerate

### Alt 1: Pre-Format Output in Lua Scripts
**Idea**: Far ritornare output già formattati dagli script Lua
**Pro**: Zero costo API extra
**Contro**:
- Ogni script deve implementare formatting
- Non scalabile
- LLM sa formattare meglio del codice

**Decisione**: ❌ Scartata

### Alt 2: Streaming Output Refinement
**Idea**: Stream output refinement in tempo reale
**Pro**: UX migliore (no attesa)
**Contro**:
- Complessità implementativa alta
- API OpenAI streaming richiede gestione SSE
- Marginal benefit vs complessità

**Decisione**: 🤔 Considerare in futuro se latency diventa issue

### Alt 3: Local LLM per Refinement
**Idea**: Usare LLM locale (TinyLlama) solo per formatting
**Pro**: Zero costo API, bassa latenza
**Contro**:
- Qualità output inferiore
- RAM ESP32 insufficiente per LLM on-device
- Complexity gestione modello

**Decisione**: ❌ Scartata per hardware constraints

---

## 📈 Metriche Successo

### KPI Post-Implementazione Fase 1
- ✅ **User Satisfaction**: Output comprensibili al 95%+
- ✅ **Latency**: Refinement < 2 secondi in 90% casi
- ✅ **Cost**: < $0.01 per sessione utente (20 query)
- ✅ **Code Quality**: Nessuna regressione, tutti test passano

### KPI Tecnici
- ✅ Copertura test > 80% per nuovo codice
- ✅ Memoria libera ESP32 > 30% post-feature
- ✅ Nessun memory leak in test 1000 query
- ✅ Error rate < 1% su refinement requests

---

## 🚀 Piano Esecuzione Consigliato

### Sessione 1: Core Refinement (2-3 ore)
```
1. [30min] Task 1.1 - Estendi VoiceCommand struct
2. [1h]    Task 1.2 - Implementa refineCommandOutput()
3. [1h]    Task 1.3 - Integra nel pipeline
4. [30min] Basic testing con query meteo
```

### Sessione 2: Smart Decision (1-2 ore)
```
1. [1h]    Task 2.1 - LLM-driven refinement flag
2. [30min] Update system prompt
3. [30min] Testing decisioni LLM
```

### Sessione 3: Context Enhancement (2 ore)
```
1. [30min] Task 4.1 - Estendi ConversationBuffer
2. [1h]    Task 4.2 - Implement context window
3. [30min] Multi-turn testing
```

### Sessione 4: Validation (1-2 ore)
```
1. [1h]    Full integration test suite
2. [30min] Performance benchmarking
3. [30min] Documentation update
```

---

## ✅ Checklist Pre-Implementazione

Prima di iniziare Fase 1, verificare:
- [x] ESP32 connesso a WiFi (serve per API calls)
- [x] API OpenAI key configurata in settings
- [x] ConversationBuffer funziona correttamente
- [x] WebDataManager testato e funzionante
- [x] Logger configurato per debug
- [x] Heap ESP32 > 50KB libero (per buffer processing)

---

## 📝 Note Finali

### Architettura Post-Miglioramento
```
User Speech
    ↓
Recording → STT (Whisper) → AI Processing
                                ↓
                    Command Execution → Raw Output
                                ↓
                    [NUOVO] Output Refinement (se necessario)
                                ↓
                    Refined Output → User Display
```

### Pattern Emergenti
L'agente diventa un **Two-Stage Agent**:
1. **Stage 1**: Command Understanding + Execution
2. **Stage 2**: Output Formatting + Presentation

Questo pattern è comune in agenti production-grade (vedi OpenAI Assistants API).

### Evolutività
Con questa base, future estensioni diventano facili:
- 🔄 Multi-step reasoning (chain commands)
- 🎨 Rich output formatting (markdown, HTML)
- 📊 Data visualization (generate charts)
- 🌐 Multi-language support (translate outputs)

---

**Documento creato**: 2025-12-04
**Versione**: 1.0
**Autore**: Claude Code (Analisi Architettura ESP32 Voice Assistant)
**Status**: Ready for Implementation ✅


## 🌦️ BONUS: API Meteo Open-Meteo per Italia

**Implementato**: 2025-12-04
**Status**: ✅ Completato

### Aggiunta al System Prompt
Documentazione completa per Open-Meteo API inclusa nel system prompt.

**Vantaggi rispetto a wttr.in:**
- ✅ JSON strutturato invece di testo semplice
- ✅ Nessuna API key richiesta  
- ✅ Coordinate pre-configurate per 13 città italiane
- ✅ Timezone Europe/Rome già impostato
- ✅ Perfetto per refinement system

**Documentazione completa:** `docs/WEATHER_API_GUIDE.md`

**Coordinate Città Disponibili:**
- Milano, Roma, Napoli, Torino, Firenze, Venezia, Bologna
- Genova, Palermo, Bari, Pisa, Livorno, Piombino

**Esempio Query LLM:**
```json
{
  "command": "lua_script",
  "args": ["webData.fetch_once('https://api.open-meteo.com/v1/forecast?latitude=42.9242&longitude=10.5267&current=temperature_2m,wind_speed_10m&timezone=Europe/Rome', 'weather_piombino.json'); println(webData.read_data('weather_piombino.json'))"],
  "text": "Sto controllando il meteo a Piombino",
  "should_refine_output": true,
  "refinement_extract": "text"
}
```

**Output Refinement trasforma:**
```json
{"current": {"temperature_2m": 12.5, "wind_speed_10m": 8.3}}
```

**In:**
`A Piombino ci sono 12.5°C con vento a 8.3 km/h.`

---

