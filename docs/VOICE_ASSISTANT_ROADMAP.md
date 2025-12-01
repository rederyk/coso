# Roadmap: Local Voice Assistant Implementation

## 📋 Overview
Implementare un assistente vocale locale sempre in ascolto nel dispositivo ESP32-S3, utilizzando OpenAI API per STT e GPT, integrato con il sistema di comandi esistente.

## 🎯 Architettura Obiettivo

### Componenti Core
- **Modulo VoiceAssistant** (`src/core/voice_assistant.cpp/h`)
- **Tre task FreeRTOS**: Captura audio, STT processing, AI orchestrator
- **Integrazione HTTPS** per OpenAI API calls
- **CommandCenter** esteso con nuovi comandi vocali
- **SettingsManager** esteso per API keys storage

### Architettura dei Task
```
[Microphone] → [VoiceCaptureTask] → Audio Buffer → [STTTask] → Transcription → [AIProcessingTask] → CommandCenter
          ↓                                                            ↑                            ↓
       VAD/Wake-word                                               API Response                    TTS/Feedback
```

## 🗓️ Roadmap di Sviluppo

### Fase 1: Fondazioni Core (Sprint 1-2)
- [ ] **Estendere CommandCenter**
  - Aggiungere comandi vocali: `radio_play`, `wifi_switch`, `bt_pair`, `volume_*`, `brightness_*`
  - Comandi offline per fallback senza internet
  - Rate limiting handler per sicurezza

- [ ] **Implementare VoiceAssistant Base**
  - Classi VoiceAssistant singleton e task structure
  - QueueHandle_t voiceQueue per command dispatch
  - Integrazione SettingsManager per API keys

- [ ] **Estendere SettingsManager**
  - Aggiungere campi: `OpenAiApiKey`, `OpenAiEndpoint`, `VoiceAssistantEnabled`
  - Metodo per salvare/recuperare token OpenAI

### Fase 2: Audio Input Pipeline (Sprint 3-4)
- [ ] **VoiceCaptureTask**
  - Configurare I2S per input microphone (separato da output)
  - Abilitare ES8311 microphone config
  - Implementare Voice Activity Detection (VAD) semplice
  - Buffer management per PSRAM
  - Wake-word detection placeholder (esp826kbd per cortex_m4)

- [ ] **Integrazione Audio**
  - Modificare openESPaudio per supportare I2S input
  - Test baseline con es8311_microphone_config(true)

### Fase 3: OpenAI Integration (Sprint 5-6)
- [ ] **STTTask (Whisper API)**
  - HTTPS client per OpenAI Whisper
  - Audio encoding WAV/MP3 per transmission
  - Pagination per audio lunghi
  - Error handling: retry con backoff esponenziale

- [ ] **AIProcessingTask (GPT API)**
  - Prompt engineering per comandi strutturati JSON
  - Catalogo comandi dinamico da listCommands()
  - JSON validation e parsing con cJSON
  - Command validation e dispatch via CommandCenter

### Fase 4: Feedback e TTS (Sprint 7-8)
- [ ] **TTS Integration**
  - TTS semplice via screen display
  - Placeholder per future audio TTS
  - Feedback confermation per azioni sensibili

- [ ] **Offline Fallback**
  - Comandi locali senza API
  - Cache transcription nella SD
  - Recovery da WiFi outages

### Fase 5: Optimizzazioni e Sicurezza (Sprint 9-10)
- [ ] **Wake-word/VAD Avanzato**
  - Snowboy o Pocketsphinx embedding
  - VAD più robusto con SNR detection
  - Battery optimization (microfono off di default)

- [ ] **Sicurezza e Affidabilità**
  - API key encryption in NVS
  - Rate limiting globale
  - Command confirmation per azioni critiche
  - Memory pooling per performance

### Fase 6: Testing e Polish (Sprint 11-12)
- [ ] **Integration Testing**
  - Command coverage completo
  - Edge cases: no WiFi, API errors, memory low
  - Performance profiling su ESP32-S3

- [ ] **User Experience**
  - Visual indicators (LED, screen message)
  - Configurable sensitivity VAD
  - Silent mode option

## 📚 Documentazione Richiesta
- [ ] **VOICE_ASSISTANT_GUIDE.md**
  - Configurazione API keys
  - Calibrazione microphone
  - Troubleshooting

- [ ] **API_INTEGRATION.md**
  - Dettagli OpenAI prompts
  - Rate limiting guidelines
  - Error handling protocols

## 🛠️ Dependencies
- ESP-IDF HTTP client (già disponibile)
- cJSON library per parsing
- openESPaudio esteso per input
- Espressif VAD component (future)

## 📊 Milestones Critici
- **MVP**: Richardson su dispositivo con basic commands
- **Alpha**: Full command coverage con TTS feedback
- **Beta**: Wake-word e offline fallback funzionanti
