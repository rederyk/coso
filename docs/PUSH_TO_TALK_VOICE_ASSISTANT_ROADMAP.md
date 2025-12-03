# 🚀 Roadmap: Implementazione Push-to-Talk Voice Assistant

## 🎯 **Obiettivo Finale**
Sistema voice assistant con pulsante hold-to-talk: premi per registrare, rilascia per elaborare comando vocale.

## 📋 **Fasi di Sviluppo**

### **Fase 1: UI e Controllo Base** ✅ COMPLETATA
- [x] **Modifica pulsante UI** per eventi PRESSED/RELEASED
- [x] **Feedback visivo** durante registrazione (pulsante rosso)
- [x] **Metodi VoiceAssistant**: `startRecording()`, `stopRecordingAndProcess()`

### **Fase 2: Voice Capture Engine** ✅ FUNZIONANTE
- [x] **Modifica VoiceCaptureTask** per controllo start/stop invece di timeout
- [x] **Buffer management** per accumulo audio durante pressione
- [x] **Integrazione ES8311** microfono (es8311_microphone_config)
- [x] **Fix I2S input** - Risolto conflitto I2S_NUM_1 → I2S_NUM_0 ✅

### **Fase 3: OpenAI Integration**
- [ ] **STTTask (Whisper)**: Conversione audio→testo
- [ ] **AIProcessingTask (GPT)**: Elaborazione comandi con prompt strutturato
- [ ] **HTTP client** per API calls con retry/backoff

### **Fase 4: Command Execution**
- [ ] **JSON parsing** risposte GPT con cJSON
- [ ] **Command dispatch** al CommandCenter esistente
- [ ] **Feedback utente** post-esecuzione

### **Fase 5: Testing & Polish**
- [ ] **API key validation** nelle settings
- [ ] **Error handling** per WiFi/API failures
- [ ] **Performance optimization** e memory management

## 🎨 **UX Flow**
1. 👆 **Hold "Hold to Talk"** → Pulsante diventa rosso + microfono attivo
2. 🗣️ **Parla comando** → Audio buffer accumula
3. 👆 **Release pulsante** → Pulsante torna verde + "Processing..."
4. 🤖 **STT → GPT → Command** → Esecuzione automatica
5. ✅ **Feedback** → Mostra comando eseguito

## 🔧 **Architettura Tecnica**
```
UI Button (PRESSED) → VoiceAssistant::startRecording()
    ↓
VoiceCaptureTask (attivo) → Accumula audio in buffer
    ↓
UI Button (RELEASED) → VoiceAssistant::stopRecordingAndProcess()
    ↓
Buffer → STTTask (Whisper) → Text → AIProcessingTask (GPT) → Command → CommandCenter
```

## ⚡ **Dipendenze**
- ESP-IDF HTTP client (disponibile)
- cJSON library (da aggiungere se mancante)
- openESPaudio per ES8311 (già integrato)

## 🚨 **PROBLEMA RISOLTO**

### **Sintomi dal Test (PRE-FIX)**
- ✅ UI pulsante funziona: `startRecording()` → `ES8311 microphone enabled`
- ✅ ES8311 si abilita correttamente
- ❌ **Sempre "No audio data recorded"** - VoiceCaptureTask non accumula audio
- ✅ Microphone test normale funziona (61440 bytes registrati)

### **Diagnosi e Fix**
Il VoiceCaptureTask non riceve dati dall'I2S quando `recording_active_` è true.
**Causa identificata**: **Conflitto I2S_NUM_1** - La stessa porta I2S era usata sia per audio input che output.

**Soluzioni implementate**:
1. ✅ **Cambiato I2S port** da `I2S_NUM_1` a `I2S_NUM_0` per evitare conflitto
2. ✅ **Aggiunto logging dettagliato** in VoiceCaptureTask per debug futuro
3. ✅ **Aggiornati commenti** per riflettere la separazione I2S

### **Test Comparativo**
Microphone test normale: ✅ 61440 bytes in 1959ms
Voice Assistant: ❌ 0 bytes (sempre)

## 🎯 **Valutazione Implementazione Corrente** (Aggiornato 2025-01-02)

### **Stato Implementazione per Fase**

#### ✅ **Fase 1: UI e Controllo Base** - COMPLETATA
- Pulsante hold-to-talk con eventi PRESSED/RELEASED funzionante
- Feedback visivo (verde → rosso durante registrazione)
- Metodi VoiceAssistant::startRecording/stopRecordingAndProcess implementati

#### ✅ **Fase 2: Voice Capture Engine** - COMPLETATA
- MicrophoneManager integrato per registrazione audio
- Buffer PSRAM funzionante
- ES8311 microfono configurato correttamente
- Fix I2S_NUM_0 risolto
- File WAV salvati in `/assistant_recordings/assistant_*.wav`

#### ⚠️ **Fase 3: OpenAI Integration** - NON IMPLEMENTATA
**Codice Stub Presente ma Non Funzionante:**
- `speechToTextTask()` (voice_assistant.cpp:242) - Loop vuoto
- `makeWhisperRequest()` (voice_assistant.cpp:269) - Return false
- `makeGPTRequest()` (voice_assistant.cpp:274) - Return false
- HTTP client ESP32 non utilizzato
- Code attualmente configurate ma non processan dati

**Problemi Identificati:**
1. Nessuna chiamata HTTP effettiva alle API OpenAI
2. Audio file WAV salvato ma mai inviato a Whisper
3. Queue `audioQueue_` non utilizzata
4. Manca conversione WAV → formato Whisper (multipart/form-data)

#### ❌ **Fase 4: Command Execution** - NON IMPLEMENTATA
- `parseGPTCommand()` (voice_assistant.cpp:279) - Return false
- JSON parsing cJSON non utilizzato
- Command dispatch a CommandCenter non integrato
- Nessun feedback utente post-esecuzione

#### ❌ **Fase 5: Testing & Polish** - NON INIZIATA
- API key validation presente in settings ma non utilizzata
- Error handling WiFi/API non implementato
- Memory management da ottimizzare (buffer audio in PSRAM)

## 📊 **Milestones**
- **MVP**: Pulsante hold-to-talk funzionante con registrazione ✅ COMPLETATO
- **MVP2**: VoiceCaptureTask riceve audio dall'I2S ✅ COMPLETATO
- **Beta**: STT funzionante con comandi base ⏳ IN SOSPESO (stub code presente)
- **Release**: Full integration STT+LLM+TTS con tutti i comandi ❌ DA IMPLEMENTARE

---

# 🐳 **PROPOSTA: Architettura API Locale con Docker**

## 🎯 **Obiettivo**
Sostituire le API cloud OpenAI con stack completamente locale su PC dell'utente tramite container Docker, eliminando dipendenze cloud e costi API.

## 🏗️ **Stack Tecnologico Raccomandato**

### **1️⃣ LLM (Large Language Model) - Ollama** ⭐ CONSIGLIATO
**Perché Ollama:**
- ✅ **OpenAI API Compatible** - Drop-in replacement senza modifiche codice ESP32
- ✅ **Docker nativo** - `docker run -d -p 11434:11434 ollama/ollama`
- ✅ **Supporto GPU** (NVIDIA/AMD/Metal) per inferenza veloce
- ✅ **Gestione modelli integrata** - Download automatico modelli
- ✅ **Memoria ottimizzata** - Funziona con 8GB RAM (modelli small)
- ✅ **Attivamente mantenuto** (2025) con ampia community

**Modelli Consigliati per ESP32:**
- `llama3.2:3b` - 2GB VRAM, veloce per comandi vocali
- `phi3:mini` - 2.3GB VRAM, ottimo reasoning
- `gemma2:2b` - 1.5GB VRAM, ultraleggero

**Endpoint ESP32:**
```cpp
constexpr const char* GPT_ENDPOINT = "http://<PC_IP>:11434/v1/chat/completions";
```

**Fonti:**
- [Ollama OpenAI Compatibility](https://ollama.com/blog/openai-compatibility)
- [Local LLM Hosting Guide 2025](https://dev.to/rosgluk/local-llm-hosting-complete-2025-guide-ollama-vllm-localai-jan-lm-studio-more-1dcl)
- [Ollama Docker Deployment](https://angelo-lima.fr/2025-01-02-ollama-open-webui-local-llm-deployment-docker/)

---

### **2️⃣ STT (Speech-to-Text) - Whisper Docker** 🎤 CONSIGLIATO
**Opzioni Valutate:**

#### **Opzione A: faster-whisper-server** ⭐ MIGLIORE per ESP32
```bash
docker run -d -p 8000:8000 \
  --gpus all \
  ghcr.io/fedirz/faster-whisper-server:latest-cuda
```
**Pro:**
- ✅ API compatibile OpenAI Whisper (`/v1/audio/transcriptions`)
- ✅ 4x più veloce di whisper originale (CTranslate2 backend)
- ✅ Supporto GPU NVIDIA via CUDA
- ✅ Streaming support per bassa latenza
- ✅ Modelli pre-configurati (tiny, base, small, medium, large-v3)

**Configurazione ESP32:**
```cpp
constexpr const char* WHISPER_ENDPOINT = "http://<PC_IP>:8000/v1/audio/transcriptions";
```

#### **Opzione B: WhisperLiveKit** (2025 SOTA)
- ✅ Ultra-bassa latenza (<200ms) con streaming
- ❌ Più complesso da configurare
- ✅ Ideale per conversazioni real-time

**Fonti:**
- [Whisper Docker GPU Guide](https://egemengulpinar.medium.com/running-whisper-large-v3-on-docker-with-gpu-support-e8a5b5daabf9)
- [WhisperLiveKit GitHub](https://github.com/QuentinFuxa/WhisperLiveKit)

---

### **3️⃣ TTS (Text-to-Speech) - OpenedAI-Speech** 🔊 CONSIGLIATO
**Stack: Piper + Coqui XTTS v2 con API OpenAI**

```bash
docker run -d -p 8001:8000 \
  ghcr.io/matatonic/openedai-speech:latest
```

**Perché openedai-speech:**
- ✅ **OpenAI TTS API compatible** (`/v1/audio/speech`)
- ✅ **Dual backend**: Piper (veloce) + Coqui XTTS v2 (qualità alta)
- ✅ **17 lingue supportate** (IT incluso)
- ✅ **Latenza <200ms** con Piper
- ✅ **Voice cloning** con XTTS v2 (opzionale)

**Alternative Valutate:**
- **Piper standalone** - Velocissimo ma qualità media
- **Coqui TTS** - Alta qualità ma richiede più risorse

**Configurazione ESP32:**
```cpp
constexpr const char* TTS_ENDPOINT = "http://<PC_IP>:8001/v1/audio/speech";
```

**Fonti:**
- [openedai-speech GitHub](https://github.com/matatonic/openedai-speech)
- [Coqui TTS Docker](https://docs.coqui.ai/en/latest/docker_images.html)
- [Piper TTS GitHub](https://github.com/rhasspy/piper)

---

## 🐳 **Docker Compose Setup Completo**

```yaml
# docker-compose.yml
version: '3.8'

services:
  # LLM - Ollama
  ollama:
    image: ollama/ollama:latest
    container_name: esp32-llm
    ports:
      - "11434:11434"
    volumes:
      - ollama_data:/root/.ollama
    deploy:
      resources:
        reservations:
          devices:
            - driver: nvidia
              count: all
              capabilities: [gpu]
    restart: unless-stopped
    command: serve

  # STT - Faster Whisper
  whisper:
    image: ghcr.io/fedirz/faster-whisper-server:latest-cuda
    container_name: esp32-stt
    ports:
      - "8000:8000"
    environment:
      - MODEL=base  # tiny/base/small/medium/large-v3
    deploy:
      resources:
        reservations:
          devices:
            - driver: nvidia
              count: all
              capabilities: [gpu]
    restart: unless-stopped

  # TTS - OpenedAI Speech
  tts:
    image: ghcr.io/matatonic/openedai-speech:latest
    container_name: esp32-tts
    ports:
      - "8001:8000"
    environment:
      - TTS_BACKEND=piper  # o "xtts_v2" per qualità maggiore
    volumes:
      - tts_voices:/app/voices
    restart: unless-stopped

  # Optional: WebUI per gestione Ollama
  open-webui:
    image: ghcr.io/open-webui/open-webui:main
    container_name: esp32-webui
    ports:
      - "3000:8080"
    volumes:
      - webui_data:/app/backend/data
    environment:
      - OLLAMA_BASE_URL=http://ollama:11434
    depends_on:
      - ollama
    restart: unless-stopped

volumes:
  ollama_data:
  tts_voices:
  webui_data:
```

**Avvio Stack:**
```bash
# 1. Avvia tutti i servizi
docker-compose up -d

# 2. Scarica modello LLM (esempio: Llama 3.2 3B)
docker exec -it esp32-llm ollama pull llama3.2:3b

# 3. Test endpoints
curl http://localhost:11434/v1/models  # LLM models
curl http://localhost:8002/v1/models   # Whisper models
curl http://localhost:8001/v1/voices   # TTS voices
```

---

## 🔧 **Modifiche Codice ESP32 Necessarie**

### **1. Configurazione Endpoints (voice_assistant.cpp)**
```cpp
// Cambiare da cloud a local
// constexpr const char* WHISPER_ENDPOINT = "https://api.openai.com/v1/audio/transcriptions";
// constexpr const char* GPT_ENDPOINT = "https://api.openai.com/v1/chat/completions";

// NUOVO: Endpoints locali (IP del PC Docker host)
constexpr const char* DOCKER_HOST_IP = "192.168.1.100";  // Configurabile via settings
constexpr const char* WHISPER_ENDPOINT = "http://192.168.1.51:8002/v1/audio/transcriptions";
constexpr const char* GPT_ENDPOINT = "http://192.168.1.51:11434/v1/chat/completions";
constexpr const char* TTS_ENDPOINT = "http://192.168.1.51:7778/v1/audio/speech";
```

### **2. Rimozione API Key Requirement**
```cpp
// Le API locali non richiedono autenticazione
// Opzionale: mantenere campo API key vuoto o usare "not-needed"
```

### **3. Settings Manager Update**
Aggiungere nuovi campi in `SettingsManager`:
- `localApiMode` (bool) - Switch cloud/local
- `dockerHostIp` (string) - IP del PC con Docker
- `whisperModel` (string) - Modello Whisper da usare
- `ollamaModel` (string) - Modello LLM da usare

### **4. Implementazione HTTP Clients**
I metodi stub dovranno essere implementati per chiamare gli endpoint locali:
- `makeWhisperRequest()` - POST multipart/form-data con WAV file
- `makeGPTRequest()` - POST JSON con prompt
- `makeTTSRequest()` - POST JSON, ricevi audio stream (opzionale)

---

## ⚡ **Vantaggi Architettura Locale**

### **Performance:**
- ⚡ Latenza ridotta (LAN vs Internet)
- ⚡ No rate limiting API
- ⚡ Inferenza GPU < 1s per comando

### **Privacy & Costi:**
- 🔒 Audio e comandi non lasciano la rete locale
- 💰 Zero costi ricorrenti API
- 💰 Una tantum: GPU (opzionale, funziona anche su CPU)

### **Affidabilità:**
- ✅ Funziona offline (no Internet richiesto)
- ✅ No dipendenze da servizi terzi
- ✅ Controllo completo su modelli e versioni

### **Flessibilità:**
- 🔄 Switch facile tra modelli LLM (ollama pull)
- 🔄 Fine-tuning personalizzato possibile
- 🔄 Multi-lingua out-of-the-box

---

## 📋 **Piano Implementazione Consigliato**

### **Fase 0: Setup Docker Stack** ✅ COMPLETATA
- [x] Installare Docker + NVIDIA Container Toolkit (se GPU)
- [x] Deploy docker-compose.yml
- [x] Pull modelli: `llama3.2:3b`, `whisper base`
- [x] Verificare endpoint con curl da PC

**Container Attivi:**
- Ollama LLM (porta 11434)
- faster-whisper STT (porta 8000)
- openedai-speech TTS (porta 8001)
- Open WebUI (porta 3000) - opzionale

### **Fase 1: ESP32 Settings UI** ✅ COMPLETATA
- [x] Aggiungere toggle "Local API Mode" in VoiceAssistantSettingsScreen
- [x] Input field per Whisper endpoint (cloud/local)
- [x] Input field per LLM endpoint (cloud/local)
- [x] Input field per Docker Host IP
- [x] Input field per LLM model name
- [x] Salvare configurazione in NVS/LittleFS
- [x] Auto-switch endpoint basato su localApiMode toggle
- [x] **NUOVO (2025-01-03):** Dropdown dinamico per selezione modelli Ollama
- [x] **NUOVO (2025-01-03):** Funzione fetchOllamaModels() per recupero modelli disponibili
- [x] **NUOVO (2025-01-03):** Pulsante refresh per aggiornare lista modelli
- [x] **NUOVO (2025-01-03):** Auto-refresh modelli quando cambia endpoint o modalità

**Modifiche Implementate:**
- `SettingsSnapshot` - Aggiunti campi: `whisperCloudEndpoint`, `whisperLocalEndpoint`, `llmCloudEndpoint`, `llmLocalEndpoint`, `llmModel`
- `SettingsManager` - Aggiunti getter/setter per nuovi endpoint
- `StorageManager` - Serializzazione/deserializzazione sezione `voiceAssistant` in JSON
- `VoiceAssistantSettingsScreen` - UI con 5 nuovi campi configurabili + **dropdown modelli dinamico**
- `voice_assistant.cpp` - Uso dinamico endpoint basato su `localApiMode`
- **`VoiceAssistant::fetchOllamaModels()`** - Nuovo metodo per interrogare API `/api/tags` di Ollama
- **Dropdown UI** - Sostituisce text input per LLM model, popola automaticamente da API Ollama
- **Auto-refresh** - Lista modelli si aggiorna quando cambia endpoint LLM o si attiva local mode

**UI Layout:**
1. Toggle "Voice Assistant Enabled"
2. Toggle "Use Local APIs (Docker)" - Switch cloud/local
3. Input "OpenAI API Key" (solo per cloud)
4. Input "API Endpoint" (legacy, deprecabile)
5. Input "Whisper STT Endpoint" (mostra endpoint attivo basato su toggle)
6. Input "LLM Endpoint" (mostra endpoint attivo basato su toggle)
7. **Dropdown "LLM Model"** con pulsante refresh (sostituisce text input)
   - In local mode: popola da Ollama API `/api/tags`
   - In cloud mode: lista predefinita (gpt-4, gpt-3.5-turbo, gpt-4-turbo)

**Default Values:**
- Whisper Cloud: `https://api.openai.com/v1/audio/transcriptions`
- Whisper Local: `http://192.168.1.51:8000/v1/audio/transcriptions`
- LLM Cloud: `https://api.openai.com/v1/chat/completions`
- LLM Local: `http://192.168.1.51:11434/v1/chat/completions`
- LLM Model: `llama3.2:3b`

### 📺 Voice Assistant Settings (come nello screen)
- La card principale include il bottone verde `Hold to Talk` che diventa rosso quando tieni premuto il microfono; le callback `handleTriggerPressed/Released` invocano `VoiceAssistant::startRecording()` e `stopRecordingAndProcess()`.
- Sotto ci sono due interruttori in stile LVGL: `Voice Assistant Enabled` attiva/disattiva il servizio e `Use Local APIs (Docker)` seleziona fra modalità cloud e locale.
- Gli input sono impilati uno per card con placeholder espliciti (`API Key`, `Endpoint`, `Whisper STT` ecc.) e suggerimenti per ricordare quale endpoint è attivo.
- La card `LLM Model` mostra ora una dropdown + bottone refresh che replica il look nella schermata: scelta dinamica dai tag di Ollama, aggiornamento automatico quando cambi endpoint.
- Il log della screen (es. `[VoiceAssistant] VoiceAssistant not initialized`) appare quando non hai ancora aperto questa schermata perché è in `onShow()` che chiami `VoiceAssistant::begin()`; aprila per inizializzare l’assistente e accedere ai tasti.

### ⚠️ Risoluzione: modello LLM locale mancante
- Se il log riporta `[VoiceAssistant] LLM API returned error status: 404` con `model 'llama3.2:3b' not found`, significa che il container Ollama non ha quel modello.
- Apri il terminale del container (es. `docker exec -it esp32-llm bash`) e `ollama pull llama3.2:3b` oppure scegli un modello già presente (mistral:7b, gemma2:2b) dal dropdown della Voce.
- Dopo il pull aggiorna la dropdown (`refresh models`) e salva: la card del dropdown mostra i modelli rilevati da `/api/tags`, replicando la lista che vedi nello screen.
- Se preferisci usare un altro modello per test immediato, modifica `SettingsSnapshot::llmModel` direttamente dalla schermata o dall’API; la dropdown mantiene la selezione funzionante.

### **Fase 2: HTTP Client Implementation** (3-4 giorni)
- [ ] Implementare `makeWhisperRequest()` con multipart upload
- [ ] Implementare `makeGPTRequest()` con JSON payload
- [ ] Error handling per timeout/network errors
- [ ] Retry logic con exponential backoff

### **Fase 3: Task Integration** (2 giorni)
- [ ] Completare `speechToTextTask()` - lettura WAV da file + POST Whisper
- [ ] Completare `aiProcessingTask()` - GPT prompt + JSON parsing
- [ ] Queue message flow completo

### **Fase 4: Command Dispatch** (1 giorno)
- [ ] Parser JSON response GPT con cJSON
- [ ] Integrazione con CommandCenter esistente
- [ ] Feedback UI dopo comando eseguito

### **Fase 5: Testing & Optimization** (2 giorni)
- [ ] Test end-to-end: voice → command execution
- [ ] Profiling latenza (target <3s totali)
- [ ] Memory leak check (PSRAM buffer cleanup)
- [ ] Fallback handling (Docker offline, etc.)

**Tempo Totale Stimato: 10-12 giorni**

---

## 🎯 **Priorità Implementazione**
1. **✅ Fase 0 (Docker Setup)** - Setup infrastruttura locale ✅ COMPLETATA
2. **✅ Fase 1 (Settings UI)** - UI per configurazione endpoint ✅ COMPLETATA (2025-01-03)
3. **✅ Fase 2 (HTTP Client)** - Implementazione core API calls ✅ COMPLETATA
4. **✅ Fase 3 (Task Integration)** - Pipeline STT → LLM completata ✅ COMPLETATA
5. **✅ Fase 4 (Commands)** - Command execution e feedback ✅ COMPLETATA

**STATO ATTUALE (2025-01-03):**
- Sistema voice assistant completamente funzionante
- Supporto cloud (OpenAI) e locale (Docker) con switch UI
- Endpoint configurabili dall'utente per massima flessibilità
- Pipeline completa: Audio → Whisper STT → Ollama LLM → Command Execution

---

## 🚀 **PROSSIMO TASK: Fase 2 - HTTP Client Implementation**

### **Obiettivo Fase 2**
Implementare i metodi HTTP client per comunicare con i container Docker locali (Whisper STT e Ollama LLM).

### **File da Modificare**
- `src/core/voice_assistant.cpp` - Implementare metodi stub
- `src/core/voice_assistant.h` - Aggiungere eventuali helper necessari
- `src/core/settings_manager.{h,cpp}` - Aggiungere campo `dockerHostIp`

### **Metodi da Implementare**

#### **1. `makeWhisperRequest()` - STT API Call**
**Input:** `AudioBuffer*` (WAV file già salvato su SD)
**Output:** `std::string` (testo trascritto)
**API Endpoint:** `POST http://<IP>:8000/v1/audio/transcriptions`
**Formato Request:** `multipart/form-data`
```cpp
// Esempio request body:
// - file: audio.wav (binary)
// - model: "base" (string)
// - language: "en" (opzionale)
```

**Implementazione Necessaria:**
- Leggere WAV file da SD Card (`last_recorded_file_`)
- Creare multipart/form-data request
- POST con `esp_http_client`
- Parse JSON response: `{"text": "transcribed text"}`
- Error handling: timeout, network, HTTP 4xx/5xx

#### **2. `makeGPTRequest()` - LLM API Call**
**Input:** `std::string` (prompt con testo trascritto)
**Output:** `std::string` (JSON response)
**API Endpoint:** `POST http://<IP>:11434/v1/chat/completions`
**Formato Request:** `application/json`
```json
{
  "model": "llama3.2:3b",
  "messages": [
    {
      "role": "system",
      "content": "You are a voice assistant that converts natural language to device commands. Respond with JSON: {\"command\": \"<cmd>\", \"args\": [\"<arg1>\", ...]}"
    },
    {
      "role": "user",
      "content": "<transcribed_text>"
    }
  ],
  "temperature": 0.7,
  "stream": false
}
```

**Implementazione Necessaria:**
- Costruire JSON payload con cJSON
- POST con `esp_http_client`
- Parse JSON response: `{"choices": [{"message": {"content": "{\"command\": ...}"}}]}`
- Extraire stringa comando JSON dal content
- Error handling

#### **3. `parseGPTCommand()` - JSON Parser**
**Input:** `std::string` (JSON response da GPT)
**Output:** `VoiceCommand` (struct con command + args)
**Formato Expected:**
```json
{
  "command": "volume_up",
  "args": []
}
```

**Implementazione Necessaria:**
- Parse JSON con cJSON
- Validazione campi `command` e `args`
- Costruire struct `VoiceCommand`
- Error handling per JSON malformato

### **Integrazione con CommandCenter**

**CommandCenter esistente supporta:**
- `executeCommand(name, args)` - Esegue comando registrato
- `listCommands()` - Lista comandi disponibili
- Comandi built-in: `volume_up`, `volume_down`, `brightness_up`, `brightness_down`, `led_brightness`, `radio_play`, `wifi_switch`, etc.

**Flow Completo:**
```
1. Audio WAV → makeWhisperRequest() → "increase the volume"
2. "increase the volume" → makeGPTRequest() → {"command": "volume_up", "args": []}
3. JSON → parseGPTCommand() → VoiceCommand{command="volume_up", args=[]}
4. VoiceCommand → CommandCenter::executeCommand() → Volume aumentato
```

### **Requisiti Tecnici**

**Librerie ESP32:**
- `esp_http_client.h` - HTTP client
- `cJSON.h` - JSON parsing (già in platformio.ini)
- `FS.h` / `SD.h` - Lettura WAV file

**Network:**
- WiFi già connesso (assumiamo funzionante)
- Docker host IP configurabile (default: 192.168.1.100)
- Timeout: 30s per Whisper, 10s per Ollama
- Retry: 2 tentativi con exponential backoff

**Memory Management:**
- WAV file read: buffer PSRAM (max 1MB)
- HTTP response: buffer PSRAM dinamico
- JSON parsing: cJSON allocazione interna
- Cleanup: free buffer dopo ogni request

### **Test Plan Fase 2**

**Test 1: Whisper STT**
```cpp
// Mock test (senza audio reale)
std::string test_wav_path = "/assistant_recordings/test.wav";
std::string transcription;
bool result = makeWhisperRequest(test_wav_path, transcription);
// Expected: result=true, transcription="test text"
```

**Test 2: Ollama LLM**
```cpp
std::string prompt = "Turn on the lights";
std::string response;
bool result = makeGPTRequest(prompt, response);
// Expected: result=true, response contiene JSON valido
```

**Test 3: JSON Parser**
```cpp
std::string json = "{\"command\": \"volume_up\", \"args\": []}";
VoiceCommand cmd;
bool result = parseGPTCommand(json, cmd);
// Expected: result=true, cmd.command="volume_up", cmd.args.empty()
```

**Test 4: End-to-End (Manuale)**
1. Registrare audio con pulsante "Hold to Talk"
2. Verificare log: `makeWhisperRequest()` successo
3. Verificare log: `makeGPTRequest()` successo
4. Verificare log: `parseGPTCommand()` successo
5. Verificare esecuzione comando (es: volume aumentato)

### **Stima Tempo Fase 2: 3-4 giorni**
- Giorno 1: `makeWhisperRequest()` + multipart upload
- Giorno 2: `makeGPTRequest()` + JSON request/response
- Giorno 3: `parseGPTCommand()` + integration testing
- Giorno 4: Error handling, retry logic, cleanup

### **Deliverables Fase 2**
- ✅ `makeWhisperRequest()` funzionante
- ✅ `makeGPTRequest()` funzionante
- ✅ `parseGPTCommand()` funzionante
- ✅ Error handling per tutti i failure case
- ✅ Memory leak-free (verificato con heap monitoring)
- ✅ Test manuali end-to-end superati

---

## 🎯 **PROSSIMI PASSI CONSIGLIATI** (Aggiornato 2025-01-03)

### **Stato Corrente Sistema**
✅ **Funzionalità Core Completate:**
- Voice recording con push-to-talk
- Whisper STT integration (locale e cloud)
- Ollama LLM integration con selezione modelli dinamica
- Command parsing e execution
- Settings UI completa con configurazione endpoint flessibile

⚠️ **Limitazioni Identificate:**
1. Comando "status" non esiste in CommandCenter → Error "Command not found"
2. Nessun server MCP integrato per comandi avanzati
3. Nessun feedback TTS (Text-to-Speech) dopo esecuzione comando
4. Lista comandi disponibili hardcoded nel system prompt LLM

---

### **Priorità 1: Migliorare Command System** 🔥

#### **1.1 Aggiungere Comando "status"**
Il log mostra che l'LLM riconosce correttamente il comando ma non esiste in CommandCenter.

**Implementazione:**
```cpp
// In CommandCenter::registerBuiltInCommands()
registerCommand("status", [](const std::vector<std::string>& args) -> CommandResult {
    std::string status_msg = "System OK\n";
    status_msg += "WiFi: " + String(WiFi.isConnected() ? "Connected" : "Disconnected") + "\n";
    status_msg += "Free Heap: " + String(ESP.getFreeHeap() / 1024) + "KB\n";
    status_msg += "Uptime: " + String(millis() / 1000 / 60) + " min\n";
    return CommandResult{true, status_msg};
});
```

#### **1.2 Generare Prompt System Dinamico**
Invece di hardcodare i comandi disponibili, interroga CommandCenter:

**Modifica in `makeGPTRequest()`:**
```cpp
// Costruire lista comandi da CommandCenter
std::string available_commands;
auto cmd_list = CommandCenter::getInstance().listCommands();
for (const auto& cmd : cmd_list) {
    available_commands += cmd + ", ";
}

// System prompt dinamico
std::string system_prompt =
    "You are a voice assistant. Convert commands to JSON.\n"
    "Available commands: " + available_commands + "\n"
    "Format: {\"command\": \"<cmd>\", \"args\": []}";
```

**Benefici:**
- LLM sempre aggiornato con comandi reali
- Nessun comando "phantom" suggerito
- Facilita estensibilità futura

---

### **Priorità 2: Integrare MCP Server** 🚀

#### **2.1 Cos'è MCP (Model Context Protocol)?**
MCP è un protocollo standard per permettere agli LLM di interagire con tool/servizi esterni.

**Casi d'uso per ESP32:**
- **Home automation**: Controllare dispositivi smart (Hue, Shelly, etc.)
- **Sensor reading**: Leggere dati da sensori locali
- **Calendar/reminder**: Integrare Google Calendar
- **Web search**: Ricerche web per domande knowledge-based
- **File management**: Gestire file su SD card

#### **2.2 MCP Server Consigliati per ESP32**

**Opzione A: MCP Server Docker Locale** ⭐ CONSIGLIATO
```yaml
# docker-compose.yml - aggiungere servizio
mcp-server:
  image: your-org/esp32-mcp-server:latest
  ports:
    - "8100:8100"
  environment:
    - ESP32_DEVICE_IP=192.168.1.10
  volumes:
    - ./mcp_tools:/app/tools
```

**Tool examples:**
- `get_temperature()` - Legge sensore temperatura ESP32
- `control_relay(pin, state)` - Controlla relay GPIO
- `read_sd_file(path)` - Legge file da SD
- `search_web(query)` - Ricerca web (via Brave/DuckDuckGo)

**Integrazione ESP32:**
```cpp
// In makeGPTRequest(), aggiungere campo "tools"
cJSON_AddItemToObject(root, "tools", buildMCPTools());

// Parser response con tool_calls
if (response contiene "tool_calls") {
    executeMCPTool(tool_name, tool_args);
    // Re-call LLM con result
}
```

**Opzione B: MCP Server Integrato su ESP32** (Avanzato)
Implementare un micro MCP server direttamente su ESP32:
- Webserver HTTP su porta 8080
- Endpoint `/tools` - lista tool disponibili
- Endpoint `/execute` - esegue tool con args

**Pro:**
- Nessuna dipendenza Docker
- Ultra-bassa latenza
- Controllo totale

**Contro:**
- Più memoria utilizzata
- Limitazione a tool semplici (no web search, etc.)

---

### **Priorità 3: Aggiungere TTS Feedback** 🔊

Attualmente il sistema esegue comandi ma non dà feedback vocale.

#### **3.1 Implementazione TTS Pipeline**

**Container Docker (già nella roadmap):**
```yaml
tts:
  image: ghcr.io/matatonic/openedai-speech:latest
  ports:
    - "8001:8000"
  environment:
    - TTS_BACKEND=piper  # veloce
```

**Modifica `aiProcessingTask()`:**
```cpp
// Dopo comando eseguito con successo
if (result.success) {
    std::string tts_text = "Command executed: " + cmd.command;
    playTTSResponse(tts_text);  // Nuovo metodo
}
```

**Implementazione `playTTSResponse()`:**
```cpp
bool VoiceAssistant::playTTSResponse(const std::string& text) {
    // 1. POST http://192.168.1.51:8001/v1/audio/speech
    //    Body: {"input": text, "voice": "alloy", "model": "tts-1"}
    // 2. Ricevi MP3/WAV stream
    // 3. Play via AudioManager/ES8311 output
    // 4. Return success
}
```

**Benefici:**
- Conferma vocale comandi eseguiti
- UX migliore per utente (feedback chiaro)
- Debugging facilitato (sentire cosa riconosce)

---

### **Priorità 4: Ottimizzazioni Performance** ⚡

#### **4.1 Reduce Latency End-to-End**

**Target attuale:** ~5-6 secondi (recording → execution)
**Target obiettivo:** <3 secondi

**Bottleneck identificati:**
1. **Whisper STT:** ~2.5s (base model)
   - **Fix:** Switch a `tiny` model (500ms, -20% accuracy)
   - **Fix:** Streaming STT (WhisperLiveKit) - latenza <200ms

2. **Ollama LLM:** ~2.5s (llama3.2:3b)
   - **Fix:** Switch a modello più piccolo (phi3:mini, 1.3B params)
   - **Fix:** Warm-up prompt all'avvio (prima inferenza lenta)
   - **Fix:** GPU inference (se disponibile)

3. **Network overhead:** ~0.5s
   - **Fix:** Connessione WiFi stabile (verificare RSSI)
   - **Fix:** HTTP keep-alive per riuso connessioni

#### **4.2 Memory Optimization**

**Heap monitoring:**
```cpp
// In voice_assistant.cpp - dopo ogni fase
LOG_I("Free heap: %u bytes", ESP.getFreeHeap());
LOG_I("PSRAM free: %u bytes", ESP.getFreePsram());
```

**Leak prevention:**
- Verificare delete di tutti i buffer audio
- cJSON_Delete() dopo ogni parsing
- esp_http_client_cleanup() dopo ogni request

---

### **Priorità 5: Error Handling & Resilienza** 🛡️

#### **5.1 Gestione Offline/Fallback**

**Scenario:** Docker host offline o LLM crash

**Implementazione:**
```cpp
// In makeGPTRequest() - aggiungere fallback
if (http_status != 200 && settings.localApiMode) {
    LOG_W("Local LLM failed, trying cloud fallback...");
    // Retry con endpoint cloud se API key presente
}
```

**Alternative:**
- Comandi hardcoded per fallback (es: "increase volume" → regex matching)
- Cache ultimo modello inferenza per offline basic commands

#### **5.2 Retry Logic Intelligente**

```cpp
int retry_count = 0;
const int MAX_RETRIES = 3;
int backoff_ms = 1000; // exponential backoff

while (retry_count < MAX_RETRIES) {
    if (makeWhisperRequest(...)) break;

    LOG_W("Retry %d/%d after %dms", retry_count, MAX_RETRIES, backoff_ms);
    vTaskDelay(pdMS_TO_TICKS(backoff_ms));
    backoff_ms *= 2; // 1s, 2s, 4s
    retry_count++;
}
```

---

### **Priorità 6: Testing & Validation** ✅

#### **6.1 Unit Tests (se PlatformIO supports)**
```cpp
// test_voice_assistant.cpp
TEST_CASE("fetchOllamaModels returns valid list", "[voice]") {
    std::vector<std::string> models;
    bool result = VoiceAssistant::getInstance().fetchOllamaModels(
        "http://192.168.1.51:11434", models
    );
    REQUIRE(result == true);
    REQUIRE(models.size() > 0);
}
```

#### **6.2 Integration Tests**
**Test scenarios:**
1. ✅ Recording → STT → Text transcription
2. ✅ Text → LLM → JSON command
3. ✅ JSON → CommandCenter → Execution
4. ⏳ Fallback when Docker offline
5. ⏳ API key validation for cloud mode
6. ⏳ Memory leak after 100 commands

---

## 📊 **ROADMAP AGGIORNATA - Timeline Consigliata**

### **Sprint 1: Command System (1-2 giorni)** 🔥 ALTA PRIORITÀ
- [ ] Aggiungere comando `status` in CommandCenter
- [ ] Generare prompt system dinamico da `listCommands()`
- [ ] Test end-to-end con nuovi comandi

### **Sprint 2: TTS Feedback (2-3 giorni)** 🔊 ALTA PRIORITÀ
- [ ] Setup container openedai-speech (già in docker-compose)
- [ ] Implementare `playTTSResponse()` per audio output
- [ ] Integrare in `aiProcessingTask()` dopo comando eseguito
- [ ] Test con vari messaggi (success, error, unknown)

### **Sprint 3: MCP Integration (3-5 giorni)** 🚀 MEDIA PRIORITÀ
- [ ] Scegliere MCP server (Docker locale o ESP32 integrato)
- [ ] Implementare tool execution pipeline
- [ ] Aggiungere tool di esempio (temperature sensor, GPIO control)
- [ ] Modificare LLM request per supportare tool_calls

### **Sprint 4: Performance Optimization (2-3 giorni)** ⚡ MEDIA PRIORITÀ
- [ ] Benchmark latency end-to-end
- [ ] Switch a Whisper tiny model per STT
- [ ] Warm-up LLM all'avvio
- [ ] HTTP keep-alive e connection pooling

### **Sprint 5: Error Handling (1-2 giorni)** 🛡️ BASSA PRIORITÀ
- [ ] Retry logic con exponential backoff
- [ ] Fallback cloud quando local API offline
- [ ] Heap monitoring e leak detection

### **Sprint 6: Testing & Polish (2 giorni)** ✅ CONTINUO
- [ ] Unit tests per core functions
- [ ] Integration tests end-to-end
- [ ] Documentazione utente finale
- [ ] Video demo completo

---

## 🎯 **RACCOMANDAZIONE FINALE**

**Prossimo task consigliato: Sprint 1 (Command System)**

**Motivazione:**
1. Quick win (1-2 giorni implementazione)
2. Risolve bug corrente ("status" command not found)
3. Foundation per MCP integration futura
4. Migliora UX immediately (comandi sempre aggiornati)

**Implementazione suggerita:**
1. Aggiungi comando `status` in CommandCenter (15 min)
2. Modifica `makeGPTRequest()` per prompt dinamico (30 min)
3. Test con comandi esistenti + status (15 min)
4. Commit e deploy su device (15 min)

**Dopo Sprint 1, procedere con Sprint 2 (TTS) per massimo impatto UX.**
