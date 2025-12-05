# TTS Testing Guide

## Cosa È Successo nel Tuo Test

### Log Analysis
```
[VoiceAssistant] Sending text message to LLM: fai un test con il tts
[VoiceAssistant] Parsed command: radio_play (args count: 1, text: Eseguo un test TTS usando il file audio sd:/tts_test.mp3)
[ERROR] sd:/tts_test.mp3 does not start with /
[ERROR] Command execution failed: Failed to play file
```

### Problema Identificato
L'IA ha scelto `radio_play` invece di chiamare `tts.speak()` perché:
1. ❌ Non conosceva l'API TTS (documentazione non caricata nel prompt)
2. ❌ Ha "inventato" che un file TTS esiste già
3. ❌ Ha provato a riprodurlo direttamente

### Soluzione Applicata
✅ Aggiornato `/data/voice_assistant_prompt.json`:
- Aggiunto auto-populate per caricare `docs/api/tts.json`
- Aggiunta sezione TTS nel prompt con esempi
- L'IA ora sa come e quando usare `tts.speak()`

## Setup Completo per Test TTS

### 1. Prerequisiti

#### A. TTS WebUI (server locale)
```bash
# Sul tuo PC/server
git clone https://github.com/rsxdalv/tts-generation-webui
cd tts-generation-webui
pip install -r requirements.txt
python server.py --api-port 7778
```

Verifica endpoint attivo:
```bash
curl http://localhost:7778/v1/audio/speech -X POST \
  -H "Content-Type: application/json" \
  -d '{"model":"tts-1","input":"test","voice":"alloy"}' \
  --output test.mp3
```

#### B. Settings ESP32
Nelle impostazioni del dispositivo:
```cpp
ttsEnabled = true
localApiMode = true
ttsLocalEndpoint = "http://192.168.1.51:7778/v1/audio/speech"  // IP del tuo server
ttsVoice = "alloy"
ttsModel = "tts-1"
ttsSpeed = 1.0
ttsOutputFormat = "mp3"
ttsOutputPath = "/littlefs/data/memory/audio"
```

#### C. Riavvia Voice Assistant
Dopo aver modificato il prompt, l'ESP32 deve ricaricare il sistema:
- Riavvia il dispositivo OPPURE
- Ricarica il prompt tramite web UI (se disponibile)

### 2. Test Sequence

#### Test 1: Richiesta Esplicita TTS
**Comando utente:**
```
"Parlami del meteo"
```

**Comportamento atteso:**
```lua
-- L'IA dovrebbe generare:
local response = "A Roma ci sono 20 gradi, cielo sereno"
local audio_file = tts.speak(response)

if audio_file then
    -- Opzionalmente riproduce l'audio
    radio.play(audio_file)
    return "Risposta vocale: " .. audio_file
else
    return response  -- fallback testuale
end
```

**Log atteso:**
```
[VoiceAssistant] Making TTS request for text: A Roma ci sono 20 gradi...
[VoiceAssistant] Using LOCAL TTS API at: http://192.168.1.51:7778/v1/audio/speech
[VoiceAssistant] HTTP Status: 200
[VoiceAssistant] Received 45320 bytes of audio data
[VoiceAssistant] TTS audio saved to: /littlefs/data/memory/audio/tts_20250112_143022.mp3
```

#### Test 2: TTS Semplice
**Comando utente:**
```
"Dimmi a voce: ciao mondo"
```

**Script IA atteso:**
```lua
local audio = tts.speak("Ciao mondo")
if audio then
    return "Audio generato: " .. audio
else
    return "TTS non disponibile"
end
```

#### Test 3: TTS con Riproduzione
**Comando utente:**
```
"Parlami e riproduci l'audio"
```

**Script IA atteso:**
```lua
local message = "Questa è una risposta vocale di test"
local audio = tts.speak(message)

if audio then
    -- Riproduci l'audio generato
    radio.play(audio)
    return "Riproduzione audio: " .. audio
else
    return message
end
```

#### Test 4: Verifica File Generati
**Comando utente:**
```
"Mostrami i file audio TTS salvati"
```

**Script IA atteso:**
```lua
local files = memory.list_files()
local tts_files = {}

for file in files:gmatch("[^\n]+") do
    if file:match("tts_.*%.mp3") then
        table.insert(tts_files, file)
    end
end

if #tts_files > 0 then
    return "File TTS: " .. table.concat(tts_files, ", ")
else
    return "Nessun file TTS trovato"
end
```

### 3. Debugging

#### Check 1: TTS Abilitato
```lua
-- Script di test
local status = system.status()
println("TTS enabled: " .. tostring(status.tts_enabled))
```

#### Check 2: WiFi Connesso
```lua
local status = system.status()
if not status.wifi_connected then
    return "WiFi non connesso!"
end
```

#### Check 3: Endpoint Raggiungibile
```bash
# Dal tuo PC, prova a chiamare l'API
curl http://192.168.1.51:7778/v1/audio/speech -X POST \
  -H "Content-Type: application/json" \
  -d '{"model":"tts-1","input":"test ESP32","voice":"alloy"}' \
  --output test_esp32.mp3
```

#### Check 4: Prompt Caricato
Dopo riavvio, verifica nei log:
```
[VoiceAssistant] Auto-populating with command: lua_exec
[LUA] Script executed successfully
📢 TTS API:
{"namespace":"tts (Text-to-Speech)",...}
```

### 4. Errori Comuni

#### Errore: TTS non disponibile
**Causa:** `ttsEnabled = false`
**Soluzione:** Abilita TTS nelle settings

#### Errore: HTTP request failed
**Causa:** TTS WebUI non raggiungibile o non attivo
**Soluzione:**
- Verifica IP corretto in `ttsLocalEndpoint`
- Controlla che TTS WebUI sia in esecuzione
- Testa endpoint con curl

#### Errore: WiFi not connected
**Causa:** ESP32 non connesso alla rete
**Soluzione:** Connetti WiFi prima di usare TTS

#### Errore: Failed to open file for writing
**Causa:** Path di output non valido o filesystem pieno
**Soluzione:**
- Verifica `ttsOutputPath` corretto
- Controlla spazio disponibile in LittleFS

### 5. Esempio Completo End-to-End

```lua
-- Script IA completo per "parlami del meteo"

-- 1. Verifica prerequisiti
local status = system.status()
if not status.wifi_connected then
    return "WiFi non connesso, impossibile usare TTS"
end

-- 2. Genera messaggio
local weather_info = "A Roma ci sono 20 gradi, cielo sereno"

-- 3. Genera audio TTS
local audio_file = tts.speak(weather_info)

-- 4. Gestisci risultato
if audio_file then
    -- TTS riuscito, opzionalmente riproduci
    local play_result = radio.play(audio_file)

    if play_result then
        return "Risposta vocale riprodotta: " .. audio_file
    else
        return "Audio generato (riproduzione fallita): " .. audio_file
    end
else
    -- TTS fallito, usa fallback testuale
    return weather_info .. " (TTS non disponibile)"
end
```

### 6. Next Steps After Successful Test

Una volta che TTS funziona:

1. **Integra con AudioManager** per riproduzione automatica
2. **Aggiungi cache** per testi ripetuti
3. **Cleanup automatico** vecchi file TTS
4. **Web UI controls** per gestire TTS settings
5. **Voice selection** dinamica dall'IA

## Summary

Il problema iniziale era che l'IA non conosceva l'API TTS. Ora:
- ✅ Documentazione TTS caricata nel prompt
- ✅ Esempi chiari di utilizzo
- ✅ L'IA sa quando e come usare `tts.speak()`
- ✅ Sistema pronto per test completo

Riavvia l'ESP32 e riprova con: **"Parlami del meteo"** 🎙️


# scriito da umano
 - consigli e test by human 
## script lua di test

local audio, err = tts.speak("Ciao, questo è un test vocale")

if audio then
println("File: " .. audio)
return audio
end

println("Errore: " .. tostring(err))
local ok, status = system.status()
if ok and status then
println(status)
else
println("system.status fallito")
end

## richieste testate per tts webui

curl -X POST http://192.168.1.51:7778/v1/audio/speech \
     -H "Content-Type: application/json" \
     -d '{"model":"hexgrad/Kokoro-82M","input":"Ciao, questo è un test vocale","voice":"af_heart","speed":1}' \
     --output output.mp3


curl -X POST http://192.168.1.51:7778/v1/audio/speech \
     -H "Content-Type: application/json" \
     -d '{"model":"hexgrad/Kokoro-82M","input":"Ciao, questo è un test vocale","voice":"if_sara","speed":1}' \
     --output output.mp3