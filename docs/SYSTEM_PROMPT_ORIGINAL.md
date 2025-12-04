La definizione del prompt di sistema è ora esterna e risiede su LittleFS in `data/voice_assistant_prompt.json`.
Questa risorsa contiene due campi principali:

- `prompt_template`: il template base con i placeholder `{{COMMAND_LIST}}` e `{{BLE_HOSTS}}`, helper BLE, esempi e la richiesta di risposta JSON.
- `sections`: un array di blocchi testo supplementari (API, pattern d'uso, fase 2, ecc.) che vengono appesi al prompt finale.

Durante l'esecuzione `VoiceAssistant::getSystemPrompt()` segue questi passi:
1. Se l'utente ha configurato un template personalizzato (`voiceAssistantSystemPromptTemplate`), lo usa al posto del template predefinito.
2. Altrimenti prende il template e le sezioni da `/voice_assistant_prompt.json`; se il file non è disponibile viene usato il fallback `VOICE_ASSISTANT_FALLBACK_PROMPT_TEMPLATE` con i placeholder.
3. Sostituisce `{{COMMAND_LIST}}` e `{{BLE_HOSTS}}` con le liste correnti fornite da `CommandCenter` e `BleHidManager`.
4. Appende tutti i blocchi testuali contenuti in `sections`.

Aggiorna la descrizione del prompt modificando `data/voice_assistant_prompt.json` e caricandolo su LittleFS; il sistema userà sempre la versione più recente salvata sul filesystem.
