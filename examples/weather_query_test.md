# 🌦️ Test Query Meteo - Open-Meteo API

## Test 1: Meteo Milano (Base)

### Query Utente
```
"Che tempo fa a Milano?"
```

### Expected LLM Response
```json
{
  "command": "lua_script",
  "args": [
    "webData.fetch_once('https://api.open-meteo.com/v1/forecast?latitude=45.4642&longitude=9.1900&current=temperature_2m,relative_humidity_2m,wind_speed_10m&timezone=Europe/Rome', 'weather_milano.json'); println(webData.read_data('weather_milano.json'))"
  ],
  "text": "Sto controllando il meteo a Milano",
  "should_refine_output": true,
  "refinement_extract": "text"
}
```

### Expected Raw Output (from Lua)
```json
{
  "latitude": 45.4642,
  "longitude": 9.1900,
  "timezone": "Europe/Rome",
  "current": {
    "time": "2025-01-03T15:00",
    "temperature_2m": 9.5,
    "relative_humidity_2m": 75,
    "wind_speed_10m": 12.5
  }
}
```

### Expected Refined Output (to User)
```
A Milano ci sono 9.5°C con umidità al 75% e vento a 12.5 km/h.
```

---

## Test 2: Meteo Piombino (Con Weather Code)

### Query Utente
```
"Com'è il tempo a Piombino?"
```

### Expected LLM Response
```json
{
  "command": "lua_script",
  "args": [
    "webData.fetch_once('https://api.open-meteo.com/v1/forecast?latitude=42.9242&longitude=10.5267&current=temperature_2m,weather_code,wind_speed_10m&timezone=Europe/Rome', 'weather_piombino.json'); println(webData.read_data('weather_piombino.json'))"
  ],
  "text": "Sto controllando il meteo a Piombino",
  "should_refine_output": true,
  "refinement_extract": "text"
}
```

### Expected Raw Output
```json
{
  "current": {
    "temperature_2m": 12.5,
    "weather_code": 2,
    "wind_speed_10m": 8.3
  }
}
```

### Expected Refined Output
```
A Piombino ci sono 12.5°C, cielo nuvoloso e vento a 8.3 km/h.
```

---

## Test 3: Meteo Salvato (Lettura File Esistente)

### Query Utente
```
"Dimmi il meteo di Milano che hai salvato"
```

### Expected LLM Response
```json
{
  "command": "lua_script",
  "args": [
    "println(webData.read_data('weather_milano.json'))"
  ],
  "text": "Ecco il meteo di Milano",
  "should_refine_output": true,
  "refinement_extract": "text"
}
```

### Expected Behavior
- ✅ Legge file esistente senza scaricarlo di nuovo
- ✅ Refine il JSON salvato
- ✅ Presenta output user-friendly

---

## Test 4: Meteo Multiplo (Due Città)

### Query Utente
```
"Confronta il meteo di Roma e Milano"
```

### Expected LLM Response
```json
{
  "command": "lua_script",
  "args": [
    "webData.fetch_once('https://api.open-meteo.com/v1/forecast?latitude=45.4642&longitude=9.1900&current=temperature_2m&timezone=Europe/Rome', 'weather_milano.json'); webData.fetch_once('https://api.open-meteo.com/v1/forecast?latitude=41.9028&longitude=12.4964&current=temperature_2m&timezone=Europe/Rome', 'weather_roma.json'); local milano = webData.read_data('weather_milano.json'); local roma = webData.read_data('weather_roma.json'); println('Milano: ' .. milano .. '\\nRoma: ' .. roma)"
  ],
  "text": "Sto confrontando il meteo di Milano e Roma",
  "should_refine_output": true,
  "refinement_extract": "text"
}
```

### Expected Refined Output
```
A Milano ci sono 9.5°C mentre a Roma 14.2°C. Roma è più calda di circa 5 gradi.
```

---

## Test 5: Errore - Città Sconosciuta

### Query Utente
```
"Che tempo fa a Pippo?"
```

### Expected LLM Response
```json
{
  "command": "none",
  "args": [],
  "text": "Mi dispiace, non conosco la città 'Pippo'. Posso aiutarti con: Milano, Roma, Napoli, Torino, Firenze, Venezia, Bologna, Genova, Palermo, Bari, Pisa, Livorno o Piombino.",
  "should_refine_output": false
}
```

---

## Test 6: Aggiornamento Schedulato

### Query Utente
```
"Aggiorna il meteo di Milano ogni 30 minuti"
```

### Expected LLM Response
```json
{
  "command": "lua_script",
  "args": [
    "webData.fetch_scheduled('https://api.open-meteo.com/v1/forecast?latitude=45.4642&longitude=9.1900&current=temperature_2m,wind_speed_10m&timezone=Europe/Rome', 'weather_milano.json', 30)"
  ],
  "text": "Ho impostato l'aggiornamento automatico del meteo di Milano ogni 30 minuti",
  "should_refine_output": false
}
```

---

## Verifica Logs

### Logs Attesi per Test Riuscito

```
[INFO] [VoiceAssistant] Lua command output: {JSON data}
[INFO] [VoiceAssistant] Lua webData/memory command, needs refinement
[INFO] [VoiceAssistant] Using heuristic for refinement decision: true
[INFO] [VoiceAssistant] Lua output needs refinement, processing...
[INFO] [VoiceAssistant] Refining command output (size: X bytes, extract: text)
[INFO] [VoiceAssistant] Making Ollama/GPT request
[INFO] [VoiceAssistant] HTTP Status: 200
[INFO] [VoiceAssistant] Extracted field 'text' from refinement: A Milano ci sono...
[INFO] [VoiceAssistant] Using refined output: A Milano ci sono...
```

### Errori Comuni

#### Error: "File not found or empty"
```
[E][vfs_api.cpp:105] open(): /sdcard/webdata/weather_milano.json does not exist
```
**Causa**: File non scaricato correttamente
**Fix**: Verificare connessione internet e permessi storage

#### Error: "Failed to parse refinement JSON"
```
[W][VoiceAssistant] Failed to parse refinement JSON, using raw response as fallback
```
**Causa**: Refinement LLM non ha restituito JSON valido
**Fix**: Sistema usa graceful fallback, output raw mostrato

---

## Metriche di Successo

- ✅ **Latenza totale** < 10 secondi (fetch + refinement)
- ✅ **Refinement accuracy** > 95% (output user-friendly)
- ✅ **Storage usage** < 5KB per file meteo
- ✅ **API calls** = 2 (fetch + refinement) per query

---

## Note Testing

1. Prima di testare, verificare WiFi connesso
2. Controllare spazio disponibile su SD/LittleFS
3. Monitorare heap ESP32 durante refinement
4. Testare con diverse città per verificare coordinate
5. Verificare timezone corretto (Europe/Rome)
