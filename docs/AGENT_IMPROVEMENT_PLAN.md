# 🧭 Piano di Miglioramento Agente (versione condensata)

## Stato attuale
- **Fase 1 – Output Refinement**: ✅ completato (refined output salvato su `ConversationBuffer`, pipeline aggiornata).
- **Fase 2 – Smart Decision**: ✅ completato (flag `should_refine_output` gestito dall’LLM con fallback euristico e prompt aggiornato).
- **Fase 3 – Multi-Instance**: ⚠️ opzionale/remoto, valutare solo se Fase 1+2 non bastano.
- **Fase 4 – Persistent Context**: 🔄 in corso; obiettivo principale del prossimo sprint.
- **Fase 5 – Testing & Validation**: 🧪 rimane da eseguire su device reale dopo il completamento della Fase 4.

## Fase 4: Persistent Context Enhancement (1 sessione)
1. **ConversationBuffer persistente**: `ConversationEntry` conserva ora `refined_output`, serializzazione e deserializzazione aggiornate (vedi `src/core/conversation_buffer.{h,cpp}`).
2. **Finestra di contesto**: quando si costruisce il prompt (`makeGPTRequest`), si includono le ultime 5 voci della `ConversationBuffer`, preferendo `refined_output` quando presente.
3. **Sistema LLM**: l’LLM riceve sempre uno storico pulito (non ingrato/JSON grezzo) e può fare riferimenti a richieste precedenti senza ricalcolare la cache.
4. **Metriche da verificare**: 5+ turni mantenuti senza drift, file `/assistant_conversation.json` aggiornato, refined output riutilizzato nelle risposte.

## Prompt e documentazione
- Riduci il prompt a due parti: (a) cosa può fare l’agente (comandi predefiniti e accesso ai documenti), (b) come riempire il JSON di risposta.
- Nel prompt, chiarisci che `docs.reference.cities()` restituisce una stringa JSON e *sempre* va decodificata tramite `local cities = cjson.decode(docs.reference.cities())` prima di leggere `cities.Piombino`.
- Mostra un esempio minimale:
  ```lua
  local cities = cjson.decode(docs.reference.cities())
  local lat, lon = cities.Piombino.lat, cities.Piombino.lon
  webData.fetch_once("https://api.open-meteo.com/v1/forecast?latitude=" .. lat .. "&longitude=" .. lon .. "&current=temperature_2m&timezone=Europe/Rome", "weather_piombino.json")
  local data = webData.read_data("weather_piombino.json")
  ```
- Elimina lunghe sezioni descrittive e mantiene solo i link più importanti: `docs.reference.cities`, `docs.reference.weather`, `docs.api.webData()`, `docs.examples.weather_query()`.

## Prossimi passi
1. Verifica che il file `/assistant_conversation.json` contenga `refined_output` e che al riavvio venga ricaricato nella request per mantenere il contesto.
2. Testa una conversazione multi-turn (“Che tempo fa?” → “E domani?”) e assicurati che l’ultima risposta usi il `refined_output` precedente.
3. Quando la Fase 4 è stabile, esegui i test di validazione della Fase 5 (performance, latenza e accuratezza delle risposte refine).
