# 🌦️ Guida API Meteo per l'Italia

## Open-Meteo API

**Vantaggi:**
- ✅ **Gratuita** - nessuna API key richiesta
- ✅ **JSON strutturato** - facile da parsare
- ✅ **Timezone Europe/Rome** - già configurato
- ✅ **Dati in tempo reale** - aggiornati ogni 15 minuti
- ✅ **Nessun rate limit** - per uso ragionevole

## URL Base

```
https://api.open-meteo.com/v1/forecast?latitude=LAT&longitude=LON&current=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,weather_code,wind_speed_10m&timezone=Europe/Rome
```

## 📍 Coordinate Città Italiane

| Città | Latitudine | Longitudine |
|-------|-----------|-------------|
| Milano | 45.4642 | 9.1900 |
| Roma | 41.9028 | 12.4964 |
| Napoli | 40.8518 | 14.2681 |
| Torino | 45.0703 | 7.6869 |
| Firenze | 43.7696 | 11.2558 |
| Venezia | 45.4408 | 12.3155 |
| Bologna | 44.4949 | 11.3426 |
| Genova | 44.4056 | 8.9463 |
| Palermo | 38.1157 | 13.3615 |
| Bari | 41.1171 | 16.8719 |
| Pisa | 43.7228 | 10.4017 |
| Livorno | 43.5485 | 10.3106 |
| Piombino | 42.9242 | 10.5267 |

## 📊 Struttura Response JSON

```json
{
  "latitude": 45.4642,
  "longitude": 9.1900,
  "timezone": "Europe/Rome",
  "current": {
    "time": "2025-01-03T15:00",
    "temperature_2m": 9.5,
    "relative_humidity_2m": 75,
    "apparent_temperature": 7.2,
    "precipitation": 0.0,
    "weather_code": 2,
    "wind_speed_10m": 12.5
  }
}
```

### Campi Disponibili

| Campo | Descrizione | Unità |
|-------|-------------|-------|
| `temperature_2m` | Temperatura a 2m dal suolo | °C |
| `relative_humidity_2m` | Umidità relativa | % |
| `apparent_temperature` | Temperatura percepita | °C |
| `wind_speed_10m` | Velocità vento a 10m | km/h |
| `precipitation` | Precipitazioni | mm |
| `weather_code` | Codice condizioni meteo | - |

### Weather Codes

| Codice | Condizione |
|--------|-----------|
| 0 | Sereno |
| 1, 2, 3 | Parzialmente nuvoloso, nuvoloso |
| 45, 48 | Nebbia |
| 51-57 | Pioviggine |
| 61-65 | Pioggia |
| 66-67 | Pioggia gelata |
| 71-77 | Neve |
| 80-82 | Rovesci di pioggia |
| 85-86 | Rovesci di neve |
| 95-99 | Temporale |

## 🤖 Esempi per LLM

### Esempio 1: Download meteo Milano

**Comando LLM:**
```json
{
  "command": "lua_script",
  "args": ["webData.fetch_once('https://api.open-meteo.com/v1/forecast?latitude=45.4642&longitude=9.1900&current=temperature_2m,relative_humidity_2m,wind_speed_10m&timezone=Europe/Rome', 'weather_milano.json')"],
  "text": "Sto scaricando i dati meteo di Milano",
  "should_refine_output": true,
  "refinement_extract": "text"
}
```

### Esempio 2: Lettura dati meteo salvati

**Comando LLM:**
```json
{
  "command": "lua_script",
  "args": ["println(webData.read_data('weather_milano.json'))"],
  "text": "Ecco il meteo di Milano",
  "should_refine_output": true,
  "refinement_extract": "text"
}
```

**Output Refinement riceverà:**
```json
{
  "latitude": 45.4642,
  "longitude": 9.1900,
  "current": {
    "temperature_2m": 9.5,
    "relative_humidity_2m": 75,
    "wind_speed_10m": 12.5
  }
}
```

**Output User-Friendly (dopo refinement):**
```
A Milano ci sono 9.5°C con umidità al 75% e vento a 12.5 km/h.
```

### Esempio 3: Meteo Piombino

**Comando LLM:**
```json
{
  "command": "lua_script",
  "args": ["webData.fetch_once('https://api.open-meteo.com/v1/forecast?latitude=42.9242&longitude=10.5267&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code&timezone=Europe/Rome', 'weather_piombino.json'); println(webData.read_data('weather_piombino.json'))"],
  "text": "Sto controllando il meteo a Piombino",
  "should_refine_output": true,
  "refinement_extract": "text"
}
```

## 🔧 Parametri URL Opzionali

### Aggiungere Previsioni Future

Per includere previsioni orarie:
```
&hourly=temperature_2m,precipitation_probability
```

### Aggiungere Dati Giornalieri

Per includere min/max giornalieri:
```
&daily=temperature_2m_max,temperature_2m_min,precipitation_sum
```

### Esempio Completo con Previsioni

```
https://api.open-meteo.com/v1/forecast?latitude=45.4642&longitude=9.1900&current=temperature_2m,relative_humidity_2m,wind_speed_10m&daily=temperature_2m_max,temperature_2m_min,precipitation_sum&timezone=Europe/Rome
```

## 🐛 Troubleshooting

### Errore: "File not found or empty"

**Causa:** Il file non è stato scaricato correttamente o il path è errato.

**Soluzione:**
1. Verifica che `webData.fetch_once()` sia stato chiamato prima
2. Controlla i log per errori HTTP
3. Assicurati che lo storage sia accessibile

### Errore: "no permits for creation"

**Causa:** StorageAccessManager blocca la scrittura nella directory `/webdata/`

**Soluzione:**
1. Verifica che la whitelist in `settings_manager.cpp` includa `/webdata/`
2. Usa il filename corretto (es: `weather_milano.json`, non path assoluto)

## 📝 Best Practices

1. **Usa nomi file descrittivi**: `weather_milano.json` invece di `weather.json`
2. **Includi timezone**: Sempre usare `&timezone=Europe/Rome`
3. **Richiedi solo dati necessari**: Non sovraccaricare l'API
4. **Cache i dati**: Usa `fetch_scheduled()` per aggiornamenti periodici
5. **Gestisci errori**: Controlla sempre se il file esiste prima di leggerlo

## 🔄 Aggiornamenti Schedulati

Per aggiornare automaticamente ogni 30 minuti:

```lua
webData.fetch_scheduled('https://api.open-meteo.com/v1/forecast?latitude=45.4642&longitude=9.1900&current=temperature_2m,relative_humidity_2m,wind_speed_10m&timezone=Europe/Rome', 'weather_milano.json', 30)
```

## 📚 Documentazione Ufficiale

Per parametri avanzati, consulta:
https://open-meteo.com/en/docs
