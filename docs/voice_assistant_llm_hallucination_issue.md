# Issue: LLM Hallucination in Voice Assistant Responses

## Descrizione del Problema
Dai log e dal feedback fornito: `{"reason":"Parameter 'latitude' and 'longitude' must have the same number di elements","error":true}`, emerge che l'LLM (Ollama con modello llama3.2:latest) sta generando risposte che includono dati inventati o non verificati. Nell'esempio della query meteo per Roma:

- L'utente chiede: "Sono italiano, dimmi che tempo fa a Roma?"
- L'LLM genera un comando JSON: `{"command":"webData.fetch_once","args":["https://api.open-meteo.com/v1/forecast?latitude=41.8718","weather.json"],"text":"Il tempo a Roma oggi è di sole con una temperatura massima di 23°C e una minima di 15°C..."}`
- Il comando `webData.fetch_once` fallisce (non implementato o non trovato).
- L'URL API è incompleto: manca il parametro `longitude` (per Roma: ~12.5674). Questo causa l'errore API quando provato.
- Il testo della risposta descrive il meteo specifico senza aver effettuato il fetch reale, indicando un'allucinazione da parte del modello LLM.

L'utente nota: "mi sembra che si stia inventando le risposte?" – Sì, l'LLM genera contenuti plausibili ma non basati su dati reali, poiché il flusso di esecuzione del comando fallisce.

## Cause Probabili
1. **Prompt System Insufficiente**: Il system prompt istruisce a rispondere SEMPRE con JSON contenente command, args, text, etc., ma non enfatizza la necessità di dati accurati o verifica. L'LLM riempie il campo "text" con una risposta inventata invece di aspettare il fetch.
2. **Comando Non Implementato**: `webData.fetch_once` è suggerito ma non eseguito correttamente nel sistema (errore: "Command not found"). Senza esecuzione, non c'è integrazione con API reali.
3. **Mancanza di Validazione**: Nessun controllo sui parametri generati (es. URL incompleti con solo latitude).
4. **Modello LLM**: Llama3.2 è leggero e prone a allucinazioni, specialmente su dati fattuali come meteo senza accesso esterno.

## Soluzioni Proposte
### 1. Migliorare il System Prompt
Modifica `data/voice_assistant_prompt.json` o il prompt in `src/core/voice_assistant.cpp` per:
- Istruire l'LLM a generare comandi completi e validi.
- Es.: "Per query meteo, usa coordinate complete (latitude=41.8719&longitude=12.5674 per Roma). Genera SOLO il comando se dati non disponibili; non inventare text."
- Aggiungi esempi: Per meteo, specifica URL completo.

Esempio prompt aggiornato:
```
Sei un assistente vocale per ESP32-S3. Rispondi SEMPRE con JSON {"command":"nome","args":[], "text":"risposta solo dopo esecuzione", "should_re_execute":false}.
Per comandi che richiedono fetch (meteo, notizie), genera URL validi completi. Non inventare dati: se fetch fallisce, text = "Errore nel recupero dati".
Esempi:
- Meteo Roma: {"command":"webData.fetch_once","args":["https://api.open-meteo.com/v1/forecast?latitude=41.8719&longitude=12.5674&current_weather=true&hourly=temperature_2m", "weather.json"], "text":"Attendo fetch..."}
```

### 2. Implementare il Comando `webData.fetch_once`
In `src/core/web_data_manager.cpp` (o simile):
- Aggiungi handler per fetch HTTP da URL e salva in file JSON.
- Usa `HTTPClient` ESP32 per GET/POST.
- Gestisci errori (es. parametri mancanti) e restituisci dati reali per popolare "text".
- Integra con VoiceAssistant: Dopo comando, esegui fetch e usa output per raffinare risposta LLM (chain of thought).

Esempio codice:
```cpp
void WebDataManager::fetchOnce(const String& url, const String& filename) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        writeToSD(filename, payload);
        // Trigger event per VoiceAssistant
    } else {
        logError("Fetch failed: " + String(httpCode));
    }
    http.end();
}
```

### 3. Aggiungere Validazione e Fallback
- Nel parsing comando in `voice_assistant.cpp`: Valida args (es. controlla presenza longitude in URL meteo).
- Se invalido, rigenera LLM con prompt di correzione: "Correggi: Aggiungi longitude=12.5674 all'URL."
- Fallback: Se comando fallisce, usa text generico: "Non posso recuperare i dati ora, riprova."

### 4. Coordinati Predefinite
Aggiungi mappa città in settings: 
```json
{
  "locations": {
    "Roma": {"lat": 41.8719, "lon": 12.5674}
  }
}
```
L'LLM può usare questo per generare URL corretti.

### 5. Test e Monitoraggio
- Simula query: "Che tempo fa a Roma?" e verifica se URL è completo.
- Log dettagliati per LLM output e fetch results.
- Switch a modello più accurato (es. llama3 se disponibile) o fine-tuning su prompt locali.

## Conclusioni
Questo issue è comune negli LLM embedded: bilanciare creatività e accuratezza. Implementando validazione comandi e prompt più restrittivi, il Voice Assistant diventerà più affidabile. Prossimo step: Aggiorna prompt, implementa fetch, testa con API reali.

Per report bug: Usa `/reportbug` nel sistema.
