# Open-Meteo API

URL: `https://api.open-meteo.com/v1/forecast`

### Parametri Richiesti:
- `latitude`: Latitudine della località (decimali)
- `longitude`: Longitudine della località (decimali)

### Parametri Opzionali per Dati Correnti (`current`):
- `temperature_2m`: Temperatura a 2 metri dal suolo (°C)
- `relative_humidity_2m`: Umidità relativa a 2 metri (%)
- `apparent_temperature`: Temperatura percepita (°C)
- `is_day`: Stato del giorno (0=notte, 1=giorno)
- `precipitation`: Precipitazione (mm)
- `rain`: Pioggia (mm)
- `showers`: Rovesci (mm)
- `snowfall`: Caduta di neve (cm)
- `weather_code`: Codice climatico WMO (es. 0=cielo sereno, 1=parzialmente nuvoloso, ecc.)
- `cloud_cover`: Copertura nuvolosa (%)
- `surface_pressure`: Pressione superficiale (hPa)
- `wind_speed_10m`: Velocità del vento a 10 metri (m/s)
- `wind_direction_10m`: Direzione del vento a 10 metri (°)
- `wind_gusts_10m`: Raffiche di vento a 10 metri (m/s)

### Parametri Opzionali Aggiuntivi:
- `timezone`: Fuso orario della località (es. `Europe/Rome`, `America/New_York`)
- `forecast_days`: Numero di giorni di previsione (max 16)
- `past_days`: Numero di giorni di dati storici (max 92)
- `hourly`: Variabili orarie (lista separata da virgole)
- `daily`: Variabili giornaliere (lista separata da virgole)

### Esempio di Richiesta:
`https://api.open-meteo.com/v1/forecast?latitude=45.4642&longitude=9.1900&current=temperature_2m,weather_code,wind_speed_10m&timezone=Europe/Rome`

Questo esempio recupererà la temperatura corrente, codice climatico e velocità del vento per Milano (lat: 45.4642, lon: 9.1900) con fuso orario di Roma.

### Risposta JSON (Esempio):
```json
{
  "latitude": 45.46,
  "longitude": 9.19,
  "generationtime_ms": 0.133,
  "utc_offset_seconds": 3600,
  "timezone": "Europe/Rome",
  "timezone_abbreviation": "CET",
  "elevation": 120.0,
  "current_units": {
    "time": "iso8601",
    "interval": "seconds",
    "temperature_2m": "°C",
    "weather_code": "wmo code",
    "wind_speed_10m": "m/s"
  },
  "current": {
    "time": "2023-10-27T10:00",
    "interval": 900,
    "temperature_2m": 15.2,
    "weather_code": 3,
    "wind_speed_10m": 5.8
  }
}
```

### Codici Climatici WMO (Estratto):
- `0`: Cielo sereno
- `1, 2, 3`: Parzialmente nuvoloso, nuvoloso, coperto
- `45, 48`: Nebbia, nebbia con brina
- `51, 53, 55`: Pioviggine leggera, moderata, intensa
- `56, 57`: Pioviggine gelata leggera, intensa
- `61, 63, 65`: Pioggia leggera, moderata, intensa
- `66, 67`: Pioggia gelata leggera, intensa
- `71, 73, 75`: Nevicata leggera, moderata, intensa
- `77`: Grandine
- `80, 81, 82`: Rovesci di pioggia leggeri, moderati, violenti
- `85, 86`: Rovesci di neve leggeri, intensi
- `95`: Temporale (senza grandine)
- `96, 99`: Temporale con grandine leggera, forte

Per la lista completa dei codici climatici WMO, fare riferimento alla documentazione ufficiale Open-Meteo.
