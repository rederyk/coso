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

Puoi anche inserire variabili dinamiche richiamando `VoiceAssistant::setSystemPromptVariable()` (es. da un comando custom) oppure sfruttare i placeholder già aggiornati automaticamente:
- `{{last_command_name}}` e `{{last_command_text}}` riportano rispettivamente il nome del comando appena eseguito e la risposta conversazionale dell'assistente.
- `{{last_command_raw_output}}` / `{{last_command_refined_output}}` contengono l'ultimo output tecnico grezzo e la versione raffinata, se disponibile.
- `{{command_<nome_comando>_output}}` / `{{command_<nome_comando>_refined_output}}` permettono di riutilizzare l'output (grezzo o raffinato) di uno specifico comando: il nome viene normalizzato in minuscolo e i caratteri non alfanumerici sono sostituiti con `_`.
Queste variabili sono aggiornate automaticamente dopo ogni comando che restituisce testo (es. lettura di file di memoria, fetch web o status system) e permettono di incollare queste informazioni direttamente nel system prompt prima di inviare la richiesta all'LLM.
