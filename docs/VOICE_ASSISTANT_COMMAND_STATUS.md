# Stato Comandi Voice Assistant

Panoramica dei comandi disponibili per l'assistente vocale (CommandCenter) e delle funzionalità chiave del dispositivo, con indicazione dello stato di implementazione.

## Comandi registrati nel CommandCenter

| Comando | Descrizione | Stato | Note / Fonte |
| --- | --- | --- | --- |
| `ping` | Verifica di connettività base (risponde `pong`) | ✅ Completo | Handler inline funzionante (`src/core/command_center.cpp:480-483`) |
| `uptime` | Restituisce l'uptime in secondi | ✅ Completo | Usa `esp_timer_get_time()` (`src/core/command_center.cpp:485-491`) |
| `heap` | Riporta statistiche heap/PSRAM | ✅ Completo | Recupera heap e PSRAM disponibili (`src/core/command_center.cpp:492-505`) |
| `sd_status` | Stato montaggio SD e spazio usato | ✅ Completo | Interroga `SdCardDriver` (`src/core/command_center.cpp:506-516`) |
| `log_tail` | Ultime N righe dei log bufferizzati | ✅ Completo | Supporta parametro numero righe (`src/core/command_center.cpp:517-542`) |
| `radio_play` | Seleziona stazione radio per nome | ⚠️ Placeholder | TODO logica di selezione, risposta statica (`src/core/command_center.cpp:543-554`) |
| `wifi_switch` | Cambia rete WiFi in uso | ⚠️ Placeholder | Richiede implementazione di switch reale (`src/core/command_center.cpp:555-566`) |
| `bt_pair` | Avvia pairing Bluetooth con device | ⚠️ Placeholder | Solo messaggio fittizio (`src/core/command_center.cpp:567-577`) |
| `bt_type` | Invia testo a un host BLE già accoppiato | ✅ Completo | Controlla bonding/connessione e usa `BleManager::sendText` (`src/core/command_center.cpp:578-609`) |
| `bt_send_key` | Invia un keycode HID verso host BLE | ✅ Completo | Supporta sia HID numerici sia combo descrittive (`ctrl+enter`, `enter`, ecc.) grazie al parsing interno (`src/core/command_center.cpp:610-642`, `src/core/voice_assistant_prompt.h:4-13`) |
| `bt_mouse_move` | Invia movimento mouse relativo/scroll/click | ✅ Completo | Convalida parametri e usa `BleManager::sendMouseMove` (`src/core/command_center.cpp:643-683`) |
| `bt_click` | Esegue click mouse (sinistro/destro/medio) | ✅ Completo | Traduzione alias pulsanti (`left/right/middle`) e chiamata `BleManager::mouseClick` (`src/core/command_center.cpp:684-711`) |
| `volume_up` | Aumenta volume audio di 10% | ✅ Completo | Aggiorna `AudioManager` (`src/core/command_center.cpp:714-722`) |
| `volume_down` | Diminuisce volume audio di 10% | ✅ Completo | Usa `AudioManager` (`src/core/command_center.cpp:723-731`) |
| `brightness_up` | Incrementa luminosità display | ✅ Completo | Chiama `BacklightManager` (`src/core/command_center.cpp:733-741`) |
| `brightness_down` | Riduce luminosità display | ✅ Completo | Limite minimo 10% (`src/core/command_center.cpp:742-750`) |
| `led_brightness` | Imposta luminosità LED RGB | ✅ Completo | Parsing parametro percentuale (`src/core/command_center.cpp:752-760`) |
| `system_status` | Sintesi heap/PSRAM/SD/WiFi | ⚠️ Parziale | WiFi ancora `unknown` (TODO) (`src/core/command_center.cpp:764-783`) |

Legenda stato:
- ✅ **Completo**: comando pienamente funzionante.
- ⚠️ **Parziale/Placeholder**: presente ma con logica incompleta o di test.

### Prompt Voice Assistant
- Il system prompt ora descrive ogni comando con la relativa descrizione e inserisce dinamicamente l'elenco degli host BLE accoppiati, così l'LLM può usare direttamente i MAC corretti (`src/core/voice_assistant.cpp:1116-1169`).
- Il template guida il modello nell'uso dei nuovi comandi `bt_type`, `bt_send_key`, `bt_mouse_move` e `bt_click`, accettando combo stile `ctrl+alt+delete` e alias per i pulsanti del mouse (`src/core/voice_assistant_prompt.h:4-13`).

### Memoria conversazionale
- Il `ConversationBuffer` persiste gli ultimi turni (utente/assistant) e viene ora serializzato nel payload `messages` inviato alla GPT/Ollama chat API, includendo testo e metadata dei comandi eseguiti (`src/core/voice_assistant.cpp:729-813`).
- Se per qualche motivo l'ultimo turno utente non è ancora stato salvato, il prompt corrente viene aggiunto come fallback, così il modello riceve sempre l'input più recente anche in presenza di errori di storage (`src/core/voice_assistant.cpp:729-797`).

## Funzionalità generali del dispositivo

| Funzione | Stato | Dettagli / Fonte |
| --- | --- | --- |
| UI push-to-talk (fase 1) | ✅ Completata | Pulsante hold-to-talk con feedback visivo e metodi `startRecording/stopRecordingAndProcess` attivi (`docs/PUSH_TO_TALK_VOICE_ASSISTANT_ROADMAP.md:8-12`) |
| Voice Capture Engine (fase 2) | ✅ Completata | Buffer PSRAM, ES8311 e fix I2S implementati (`docs/PUSH_TO_TALK_VOICE_ASSISTANT_ROADMAP.md:13-18,87-93`) |
| Integrazione OpenAI (fase 3) | ❌ Non implementata | Task STT/GPT e client HTTP ancora stub (`docs/PUSH_TO_TALK_VOICE_ASSISTANT_ROADMAP.md:19-23,94-107`) |
| Command dispatch + feedback (fase 4) | ❌ Non implementata | Parsing JSON e collegamento al CommandCenter mancanti (`docs/PUSH_TO_TALK_VOICE_ASSISTANT_ROADMAP.md:24-27,108-112`) |
| Testing, gestione errori e polish (fase 5) | ❌ Non iniziata | Validazione API key e gestione errori ancora da fare (`docs/PUSH_TO_TALK_VOICE_ASSISTANT_ROADMAP.md:29-32,114-117`) |
| Milestones | 🔄 In corso | MVP & VoiceCapture completati; Beta (STT) in sospeso, Release non avviata (`docs/PUSH_TO_TALK_VOICE_ASSISTANT_ROADMAP.md:119-123`) |
