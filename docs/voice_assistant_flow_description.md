# Descrizione del Flow del Voice Assistant nell'ESP32-S3 OS Dashboard

## Introduzione
Questo documento descrive il flow di esecuzione osservato nei log di avvio e interazione del sistema operativo personalizzato per ESP32-S3 (Freenove ESP32-S3 OS Dashboard). Il log copre l'avvio del sistema, l'inizializzazione dei componenti principali e le interazioni successive con il Voice Assistant, inclusi i processi di registrazione audio, trascrizione (STT) tramite Whisper e elaborazione tramite LLM (Ollama/GPT). Il sistema è configurato per l'uso locale con servizi su `192.168.1.51` (Whisper su porta 8002 e LLM su 11434).

Il log è datato 8 dicembre 2025 e mostra un boot completo con connessioni WiFi/BLE, montaggio SD, inizializzazione UI (LVGL) e test del Voice Assistant in italiano.

## Fasi Principali del Flow

### 1. Boot e Inizializzazione del Sistema
- **Caricamento Firmware**: Il boot inizia con SPIWP:0xee, mode:DIO, clock div:1. Vengono caricati segmenti di codice (load:...) e l'entry point è impostato a 0x403c98d0.
- **Informazioni Hardware e Build**:
  - Build: Dec 8 2025 10:17:35 | IDF v4.4.7-dirty
  - Chip: ESP32-S3 rev 0 | 2 core(s) @ 240 MHz
  - Flash: 16 MB QIO | PSRAM: 7 MB
- **Memoria Iniziale (Stage: Boot)**:
  - DRAM free: 288048 / 323868 bytes
  - PSRAM free: 8351975 / 8388607 bytes
- **Montaggio Storage**: LittleFS montato su /littlefs. Errore iniziale per `/littlefs/settings.json.tmp` (file non esistente, no permessi di creazione).
- **Settings Manager**: Inizializzato e salvato atomicamente. Boot count: 56. Modalità operativa: FULL.
- **BLE Manager**: Task avviato su Core 0. Indirizzo: 98:a3:16:e8:d7:a9. Advertising automatico abilitato.
- **Audio Manager**: Inizializzato con stazioni radio predefinite (Radio Paradise, Radio 105).
- **Microphone Manager**: Inizializzato.
- **WebData Manager**: Inizializzato, legge da SD (es. calendar_events.json).
- **Memoria Manager**: Inizializzato.
- **Scheduler**: Time scheduler pronto.
- **RGB LED**: Inizializzato su GPIO42 con driver RMT.
- **Voice Assistant**: Inizializzato (usa MicrophoneManager).
- **Touch Controller**: FT6336 rilevato su I2C (SDA=16, SCL=15). Vendor ID: 0x11, Chip ID: 0x64, FW: 0x02.
- **Backlight**: Inizializzato su pin 45 al 100%.
- **LVGL e Display**:
  - Memoria dopo lv_init: DRAM free 168452, PSRAM free 7957111.
  - Buffer: DRAM Single (7680 px, 15360 bytes).
  - Display: ILI9341 240x320, orientamento Portrait.
  - Capacitive touch abilitato.
- **UI Pronta**: App lanciata: Home. Memoria: DRAM free 148988.
- **Servizi Aggiuntivi**:
  - LVGL task avviato.
  - Auto-backup ogni 30 min.
  - Memory logging ogni 60s.
  - LVGL Power Manager: Modalità manuale (auto-suspend disabilitato).
- **Connessioni**:
  - BLE HID: Connesso (04:42:1a:55:c9:a3).
  - WiFi: Connesso, timezone CET-1CEST, NTP sync (pool.ntp.org, etc.).
  - RGB LED: Cambi stato per connessioni.
  - SD Card: Montata (SDHC/SDXC).
  - WebServer: Avviato su porta 80.
  - AsyncRequestManager: Avviato.

### 2. Inizializzazione Voice Assistant
- VoiceAssistant instance creata.
- Task STT e AI processing avviati.
- Memoria prima task: free=53740.
- Inizializzato con MicrophoneManager.

### 3. Prima Interazione Vocale (Trascrizione: "Моря.")
- **Registrazione Audio**: Task avviato. I2S esclusivo acquisito. Registrazione su SD: /sd/assistant_recordings/assistant_000045.wav (24620 bytes, 788 ms).
  - ES8311 configurato (sample freq, mic enabled).
  - Debug samples: -14 -12 -18 -39.
- **Processamento STT**:
  - File aperto, letto (24620 bytes).
  - Richiesta HTTP a Whisper locale: http://192.168.1.51:8002/v1/audio/transcriptions (multipart/form-data).
  - Risposta: 200, trascrizione "Моря." (probabilmente un test o errore di riconoscimento).

### 4. Seconda Interazione Vocale (Trascrizione: "con italiano parlo italiano")
- Registrazione: /sd/assistant_recordings/assistant_000046.wav (61484 bytes, 1929 ms).
  - Debug samples: -2 -10 -26 -89.
- STT: Richiesta HTTP simile, trascrizione "con italiano parlo italiano".

### 5. Terza Interazione Vocale (Trascrizione: "Sono italiano, dimmi che tempo fa a Roma?")
- Registrazione: /sd/assistant_recordings/assistant_000047.wav (110636 bytes, 3465 ms).
  - Debug samples: -7 -8 -19 -102.
- STT: Trascrizione "Sono italiano, dimmi che tempo fa a Roma?".
- **Elaborazione LLM**:
  - Richiesta queued in AsyncRequestManager.
  - Invio a Ollama locale: http://192.168.1.51:11434/v1/chat/completions.
  - System prompt: Assistente vocale ESP32-S3, rispondi sempre in JSON con command, args, text, should_re_execute.
  - Modello: llama3.2:latest.
  - Risposta JSON: Command "webData.fetch_once" con args ["https://api.open-meteo.com/v1/forecast?latitude=41.8718", "weather.json"].
  - Text: Descrizione meteo Roma (sole, 23°C max, 15°C min, poche nuvole, bassa probabilità pioggia).
  - LLM specifica refinement_extract: "temperature".
- **Esecuzione Command**: "webData.fetch_once" non trovato → Errore "Command not found".
- Richiesta completata dopo timeout/attesa.

### 6. Quarta Interazione Vocale (Trascrizione: "Anche le notizie.")
- Registrazione: /sd/assistant_recordings/assistant_000048.wav (36908 bytes, 1163 ms).
  - Debug samples: 7 -3 -15 -80.
- STT: "Anche le notizie."
- **Elaborazione LLM**:
  - Invio a Ollama.
  - Conversation history: 3 entries.
  - Risposta: Command "webData.fetch_once" con args ["https://www.anagraweb.it/", "news.json"].
  - Text: Estratti notizie (es. "Cronaca: un uomo armato ferisce diversi in una periferia di Roma").
- **Parsing**: Fallito nel parsing JSON completo → Fallback a raw response.

### 7. Osservazioni Generali
- **Memoria Periodica**: Log ogni 60s mostrano DRAM free ~27-28k, PSRAM stabile ~7.8M.
- **Errori**:
  - File non esistenti (settings.json.tmp, scheduled_tasks.json).
  - Command non trovato per webData.fetch_once.
  - Parsing JSON fallito nell'ultima interazione.
- **Ottimizzazioni**: Uso di PSRAM abilitato. Buffer LVGL in DRAM single per efficienza.
- **Dipendenze Esterne**: Dipende da servizi locali (Whisper, Ollama) e API (Open-Meteo, Anagraweb).
- **Lingua**: Interazioni in italiano, con riconoscimento multilingua (es. cirillico iniziale).

## Conclusioni
Il flow dimostra un sistema robusto per l'avvio embedded con UI touch, BLE/WiFi, audio e AI vocale. Il Voice Assistant gestisce bene la pipeline STT-LLM-Command, ma richiede implementazione di comandi come `webData.fetch_once` e miglior parsing JSON. Futuri miglioramenti: Gestione errori comandi, supporto fetch dati, storia conversazionale dinamica.

Per test: Simula interazioni vocali simili e verifica logging.
