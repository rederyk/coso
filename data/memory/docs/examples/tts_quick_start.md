# TTS Quick Start Guide

## Setup Rapido TTS WebUI

### 1. Installazione TTS WebUI (sul tuo PC/server)

```bash
git clone https://github.com/rsxdalv/tts-generation-webui
cd tts-generation-webui
pip install -r requirements.txt
python server.py --api-port 7778
```

L'API sarà disponibile su: `http://localhost:7778/v1/audio/speech`

### 2. Configurazione ESP32

Nelle settings ESP32:
```cpp
ttsEnabled = true
localApiMode = true  // usa TTS locale invece di OpenAI cloud
ttsLocalEndpoint = "http://192.168.1.51:7778/v1/audio/speech"  // IP del tuo PC
ttsVoice = "alloy"  // voce da usare
```

### 3. Primo Test

Chiedi all'assistente vocale:
```
"Parlami del meteo"
```

L'IA genererà automaticamente:
```lua
local response = "A Roma ci sono 20 gradi"
local audio_file = tts.speak(response)
return audio_file  -- es: /littlefs/data/memory/audio/tts_20250112_143022.mp3
```

## Esempi Pratici

### Esempio 1: Risposta Vocale su Richiesta

**Utente**: "Dimmi a voce che ore sono"

**Script Lua IA**:
```lua
local status = system.status()
local time = status.time

local message = "Sono le ore " .. time
local audio = tts.speak(message)

if audio then
    return "Audio salvato: " .. audio
else
    return message  -- fallback testuale
end
```

### Esempio 2: Alert Vocale Automatico

**Scenario**: Temperatura critica rilevata

**Script Lua IA**:
```lua
local sensor_temp = 85

if sensor_temp > 80 then
    local alert = "Attenzione! Temperatura critica " .. sensor_temp .. " gradi"

    -- Genera sempre audio per alert critici
    local audio = tts.speak(alert)

    -- Log evento
    memory.append_file("alerts.log", alert .. "\n")

    return audio or alert
end
```

### Esempio 3: Lettura Messaggi

**Utente**: "Leggi l'ultimo messaggio"

**Script Lua IA**:
```lua
local messages = memory.read_file("messages.txt")

if messages then
    local last = messages:match("([^\n]+)$")

    if last then
        local audio = tts.speak(last)
        return audio or last
    end
end

return "Nessun messaggio"
```

## Quando l'IA Usa TTS Automaticamente

### 1. Richiesta Esplicita Utente
Parole chiave rilevate:
- "parlami"
- "dimmi a voce"
- "leggi"
- "a voce"

### 2. TTS Globalmente Abilitato
Se `ttsEnabled = true`, l'IA può usare TTS per:
- Risposte importanti
- Notifiche
- Assistenza

### 3. Alert Critici
- Temperatura alta
- Batteria scarica
- Errori sistema

## Verifica File Audio Generati

```lua
-- Lista tutti i file TTS salvati
local files = memory.list_files()
println(files)

-- Output esempio:
-- tts_20250112_140122.mp3
-- tts_20250112_141530.mp3
-- tts_20250112_143022.mp3
```

## Gestione Errori

### WiFi Non Connesso
```lua
local status = system.status()
if not status.wifi_connected then
    return "WiFi richiesto per TTS"
end

local audio = tts.speak("test")
```

### TTS Disabilitato
```lua
local audio = tts.speak("test")
if not audio then
    return "TTS non disponibile (disabilitato in settings)"
end
```

### API Non Raggiungibile
Il sistema logga automaticamente:
```
[TTS] HTTP request failed: ESP_ERR_HTTP_CONNECT
[TTS] Check if server is reachable
```

## Configurazioni Avanzate

### Cambia Voce
```cpp
ttsVoice = "nova"  // voce femminile
ttsVoice = "onyx"  // voce maschile profonda
```

### Cambia Velocità
```cpp
ttsSpeed = 0.75f  // più lento
ttsSpeed = 1.25f  // più veloce
```

### Cambia Formato Audio
```cpp
ttsOutputFormat = "opus"  // migliore compressione
ttsOutputFormat = "flac"  // qualità massima
```

## Troubleshooting

### TTS Non Funziona

1. **Verifica WiFi connesso**:
   ```lua
   local status = system.status()
   println("WiFi: " .. (status.wifi_connected and "OK" or "NO"))
   ```

2. **Verifica TTS abilitato**:
   ```
   Check settings: ttsEnabled = true
   ```

3. **Verifica endpoint raggiungibile**:
   ```bash
   curl http://192.168.1.51:7778/v1/audio/speech -X POST -H "Content-Type: application/json" -d '{"model":"tts-1","input":"test","voice":"alloy"}'
   ```

4. **Check logs ESP32**:
   ```
   [TTS] Making TTS request for text: ...
   [TTS] Using LOCAL TTS API at: ...
   [TTS] HTTP Status: 200
   [TTS] Received ... bytes of audio data
   [TTS] TTS audio saved to: ...
   ```

## Performance

- **Latenza**: ~2-5 secondi per frase breve (dipende da rete e server)
- **Dimensione file**: ~20-50 KB per frase (~5-10 secondi audio)
- **Timeout**: 30 secondi per richiesta

## Best Practices

1. **Messaggi concisi**: TTS funziona meglio con frasi brevi e chiare
2. **Fallback testuale**: Sempre fornire alternativa testuale
3. **Cache intelligente**: Riutilizza file per frasi comuni
4. **Cleanup periodico**: Elimina vecchi file TTS per liberare spazio

## Prossimi Passi

Dopo TTS funziona, puoi:
1. Integrare riproduzione automatica tramite AudioManager
2. Creare coda TTS per multiple richieste
3. Implementare cache per testi ripetuti
4. Aggiungere UI web per gestire TTS
