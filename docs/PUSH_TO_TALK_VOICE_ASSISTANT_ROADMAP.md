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

### **Fase 1: ESP32 Settings UI** (2 giorni)
- [ ] Aggiungere toggle "Local API Mode" in VoiceAssistantSettingsScreen
- [ ] Input field per Docker Host IP
- [ ] Dropdown selezione modelli Whisper/Ollama
- [ ] Salvare configurazione in NVS/LittleFS

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
2. **🔄 NEXT: Fase 2 (HTTP Client)** - Implementazione core API calls ⬅️ **PROSSIMO STEP**
3. **⏳ Fase 3 (Task Integration)** - Completare pipeline STT → LLM
4. **⏳ Fase 1 (Settings UI)** - UI per configurazione (può essere dopo)
5. **⏳ Fase 4 (Commands)** - Command execution e feedback

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
