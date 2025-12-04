# Stato Comandi Voice Assistant

Panoramica dei comandi disponibili per l'assistente vocale (CommandCenter) e delle funzionalità chiave del dispositivo, con indicazione dello stato di implementazione.

## Comandi registrati nel CommandCenter

| Comando | Descrizione | Stato | Note / Fonte |
| --- | --- | --- | --- |
| `ping` | Verifica di connettività base (risponde `pong`) | ✅ Completo | Handler inline funzionante (`src/core/command_center.cpp:288`) |
| `uptime` | Restituisce l'uptime in secondi | ✅ Completo | Usa `esp_timer_get_time()` (`src/core/command_center.cpp:293`) |
| `heap` | Riporta statistiche heap/PSRAM | ✅ Completo | Recupera heap e PSRAM disponibili (`src/core/command_center.cpp:300`) |
| `sd_status` | Stato montaggio SD e spazio usato | ✅ Completo | Interroga `SdCardDriver` (`src/core/command_center.cpp:314`) |
| `log_tail` | Ultime N righe dei log bufferizzati | ✅ Completo | Supporta parametro numero righe (`src/core/command_center.cpp:325-346`) |
| `radio_play` | Seleziona stazione radio per nome | ⚠️ Placeholder | TODO logica di selezione, risposta statica (`src/core/command_center.cpp:351-359`) |
| `wifi_switch` | Cambia rete WiFi in uso | ⚠️ Placeholder | Richiede implementazione di switch reale (`src/core/command_center.cpp:363-372`) |
| `bt_pair` | Avvia pairing Bluetooth con device | ⚠️ Placeholder | Solo messaggio fittizio (`src/core/command_center.cpp:375-384`) |
| `bt_type` | Invia testo a un host BLE già accoppiato | ✅ Completo | Controlla bonding/connessione e usa `BleManager::sendText` (`src/core/command_center.cpp:386-415`) |
| `bt_send_key` | Invia un keycode HID verso host BLE | ✅ Completo | Parsing keycode/modifier e dispatch tramite queue BLE (`src/core/command_center.cpp:418-448`) |
| `bt_mouse_move` | Invia movimento mouse relativo/scroll/click | ✅ Completo | Convalida parametri e usa `BleManager::sendMouseMove` (`src/core/command_center.cpp:451-489`) |
| `bt_click` | Esegue click mouse (sinistro/destro/medio) | ✅ Completo | Traduzione alias pulsanti e chiamata `BleManager::mouseClick` (`src/core/command_center.cpp:492-518`) |
| `volume_up` | Aumenta volume audio di 10% | ✅ Completo | Aggiorna `AudioManager` (`src/core/command_center.cpp:522-529`) |
| `volume_down` | Diminuisce volume audio di 10% | ✅ Completo | Usa `AudioManager` (`src/core/command_center.cpp:531-538`) |
| `brightness_up` | Incrementa luminosità display | ✅ Completo | Chiama `BacklightManager` (`src/core/command_center.cpp:541-549`) |
| `brightness_down` | Riduce luminosità display | ✅ Completo | Limite minimo 10% (`src/core/command_center.cpp:550-558`) |
| `led_brightness` | Imposta luminosità LED RGB | ✅ Completo | Parsing parametro percentuale (`src/core/command_center.cpp:560-568`) |
| `system_status` | Sintesi heap/PSRAM/SD/WiFi | ⚠️ Parziale | WiFi ancora `unknown` (TODO) (`src/core/command_center.cpp:572-584`) |

Legenda stato:
- ✅ **Completo**: comando pienamente funzionante.
- ⚠️ **Parziale/Placeholder**: presente ma con logica incompleta o di test.

## Funzionalità generali del dispositivo

| Funzione | Stato | Dettagli / Fonte |
| --- | --- | --- |
| UI push-to-talk (fase 1) | ✅ Completata | Pulsante hold-to-talk con feedback visivo e metodi `startRecording/stopRecordingAndProcess` attivi (`docs/PUSH_TO_TALK_VOICE_ASSISTANT_ROADMAP.md:8-12`) |
| Voice Capture Engine (fase 2) | ✅ Completata | Buffer PSRAM, ES8311 e fix I2S implementati (`docs/PUSH_TO_TALK_VOICE_ASSISTANT_ROADMAP.md:13-18,87-93`) |
| Integrazione OpenAI (fase 3) | ❌ Non implementata | Task STT/GPT e client HTTP ancora stub (`docs/PUSH_TO_TALK_VOICE_ASSISTANT_ROADMAP.md:19-23,94-107`) |
| Command dispatch + feedback (fase 4) | ❌ Non implementata | Parsing JSON e collegamento al CommandCenter mancanti (`docs/PUSH_TO_TALK_VOICE_ASSISTANT_ROADMAP.md:24-27,108-112`) |
| Testing, gestione errori e polish (fase 5) | ❌ Non iniziata | Validazione API key e gestione errori ancora da fare (`docs/PUSH_TO_TALK_VOICE_ASSISTANT_ROADMAP.md:29-32,114-117`) |
| Milestones | 🔄 In corso | MVP & VoiceCapture completati; Beta (STT) in sospeso, Release non avviata (`docs/PUSH_TO_TALK_VOICE_ASSISTANT_ROADMAP.md:119-123`) |
