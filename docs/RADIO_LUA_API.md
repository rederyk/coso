# Radio/Audio Player - Lua API Documentation

Sistema completo per la gestione di web radio streaming e file audio locali tramite script Lua.

## Caratteristiche

- ✅ **Streaming HTTP**: Supporto completo per web radio con buffering timeshift
- ✅ **File locali**: Riproduzione da SD card e LittleFS
- ✅ **Controllo playback**: Play, pause, resume, stop, seek
- ✅ **Status dettagliato**: Informazioni complete su stato, posizione, metadata
- ✅ **Gestione volume**: Controllo volume 0-100%
- ✅ **Auto-detection**: Riconoscimento automatico URL vs file path

## API Lua

### `radio.play(url_or_path)`

Avvia la riproduzione di uno stream radio o file audio.

**Parametri:**
- `url_or_path` (string): URL stream HTTP/HTTPS oppure percorso file su SD/LittleFS

**Ritorna:**
- `success` (boolean): `true` se avviato con successo
- `message` (string): Messaggio di conferma o errore

**Esempi:**

```lua
-- Web radio stream
local ok, msg = radio.play("http://icecast.example.com/stream.mp3")
println(msg)

-- File su SD card
local ok, msg = radio.play("/sd/music/song.mp3")
println(msg)

-- File su LittleFS
local ok, msg = radio.play("/littlefs/audio/jingle.mp3")
println(msg)
```

---

### `radio.status()`

Restituisce lo stato completo del player audio.

**Parametri:** Nessuno

**Ritorna:**
- `success` (boolean): Sempre `true`
- `status_text` (string): Testo formattato con tutte le informazioni

**Output include:**
- **State**: STOPPED | PLAYING | PAUSED | ENDED | ERROR
- **Volume**: Percentuale volume corrente
- **Source**: Tipo sorgente (HTTP_STREAM, SD_CARD, LITTLEFS)
- **Title/Artist/Album**: Metadata ID3 (se disponibili)
- **Position**: Posizione riproduzione in secondi
- **Timeshift**: Indicazione buffer disponibile per stream HTTP

**Esempio:**

```lua
local ok, status = radio.status()
println(status)

-- Output esempio:
-- State: PLAYING
-- Volume: 75%
-- Source: HTTP_STREAM
-- Title: Jazz Classics
-- Artist: Various Artists
-- Position: 45s / 180s (buffered)
-- Timeshift: Available
```

---

### `radio.stop()`

Ferma completamente la riproduzione e libera le risorse.

**Parametri:** Nessuno

**Ritorna:**
- `success` (boolean): Sempre `true`
- `message` (string): "Playback stopped"

**Esempio:**

```lua
local ok, msg = radio.stop()
println(msg)  -- Output: "Playback stopped"
```

---

### `radio.pause()`

Mette in pausa la riproduzione corrente. Lo stream continua il buffering in background.

**Parametri:** Nessuno

**Ritorna:**
- `success` (boolean): Sempre `true`
- `message` (string): "Playback paused"

**Esempio:**

```lua
local ok, msg = radio.pause()
println(msg)  -- Output: "Playback paused"
```

---

### `radio.resume()`

Riprende la riproduzione dopo una pausa.

**Parametri:** Nessuno

**Ritorna:**
- `success` (boolean): Sempre `true`
- `message` (string): "Playback resumed"

**Esempio:**

```lua
local ok, msg = radio.resume()
println(msg)  -- Output: "Playback resumed"
```

---

### `radio.seek(seconds)`

Salta avanti o indietro nella riproduzione.

**Parametri:**
- `seconds` (integer): Numero di secondi da saltare (positivo = avanti, negativo = indietro)

**Ritorna:**
- `success` (boolean): Sempre `true`
- `message` (string): Conferma dello spostamento

**Esempi:**

```lua
-- Salta avanti di 30 secondi
local ok, msg = radio.seek(30)
println(msg)  -- Output: "Seeking forward 30 seconds"

-- Torna indietro di 10 secondi
local ok, msg = radio.seek(-10)
println(msg)  -- Output: "Seeking backward 10 seconds"
```

**Note:**
- Per file locali: seek assoluto all'interno del file
- Per stream HTTP timeshift: seek all'interno del buffer disponibile

---

### `radio.set_volume(volume)`

Imposta il volume di riproduzione.

**Parametri:**
- `volume` (integer): Livello volume 0-100 (viene automaticamente clampato)

**Ritorna:**
- `success` (boolean): Sempre `true`
- `message` (string): Conferma impostazione

**Esempio:**

```lua
local ok, msg = radio.set_volume(75)
println(msg)  -- Output: "Volume set to 75%"

-- Valori fuori range vengono clampati
radio.set_volume(150)  -- Imposta a 100%
radio.set_volume(-10)  -- Imposta a 0%
```

---

## Esempi Completi

### 1. Player Radio con Controlli Base

```lua
-- Avvia una web radio
local ok, msg = radio.play("http://stream.radioparadise.com/aac-320")
if ok then
    println("✓ Radio avviata")
else
    println("✗ Errore: " .. msg)
    return
end

-- Aspetta 5 secondi
delay(5000)

-- Controlla lo status
local ok, status = radio.status()
println("Status attuale:")
println(status)

-- Pausa
radio.pause()
delay(2000)

-- Riprendi
radio.resume()
delay(10000)

-- Stop finale
radio.stop()
println("Riproduzione terminata")
```

### 2. Playlist da SD Card

```lua
local playlist = {
    "/sd/music/track1.mp3",
    "/sd/music/track2.mp3",
    "/sd/music/track3.mp3"
}

for i, track in ipairs(playlist) do
    println("▶ Riproducendo: " .. track)

    local ok, msg = radio.play(track)
    if not ok then
        println("Errore: " .. msg)
    else
        -- Aspetta che finisca il brano (esempio semplificato)
        delay(180000)  -- 3 minuti
        radio.stop()
    end
end

println("Playlist completata")
```

### 3. Controllo Volume Dinamico

```lua
-- Avvia radio
radio.play("http://ice1.somafm.com/groovesalad-256-mp3")

-- Fade in graduale
for vol = 0, 80, 10 do
    radio.set_volume(vol)
    delay(500)
end

-- Riproduzione normale per 30 secondi
delay(30000)

-- Fade out graduale
for vol = 80, 0, -10 do
    radio.set_volume(vol)
    delay(500)
end

radio.stop()
```

### 4. Radio con Timeshift Seek

```lua
-- Avvia stream con timeshift
radio.play("http://stream.example.com/radio.mp3")
delay(60000)  -- Aspetta 1 minuto per buffer

-- Torna indietro di 30 secondi (riascolta)
radio.seek(-30)
delay(10000)

-- Salta avanti di 45 secondi
radio.seek(45)
delay(10000)

-- Status finale
local ok, status = radio.status()
println(status)

radio.stop()
```

### 5. Monitor Status Periodico

```lua
-- Avvia riproduzione
radio.play("http://stream.radioparadise.com/mp3-128")

-- Monitora per 2 minuti
for i = 1, 24 do  -- 24 x 5 secondi = 2 minuti
    delay(5000)

    local ok, status = radio.status()
    println("--- Aggiornamento #" .. i .. " ---")
    println(status)
    println("")
end

radio.stop()
```

### 6. Integrazione con BLE (Controllo Remoto)

```lua
-- Funzione helper per inviare notifica a host BLE
local function notify_ble(text)
    ble.type("AA:BB:CC:DD:EE:FF", text)
end

-- Avvia radio
local ok, msg = radio.play("http://stream.radioparadise.com/aac-320")
if ok then
    notify_ble("Radio started: Radio Paradise")
else
    notify_ble("Error: " .. msg)
    return
end

delay(10000)

-- Invia status via BLE
local ok, status = radio.status()
notify_ble(status)

-- Pausa con notifica
radio.pause()
notify_ble("Radio paused")

delay(5000)

-- Resume con notifica
radio.resume()
notify_ble("Radio resumed")

delay(10000)
radio.stop()
notify_ble("Radio stopped")
```

---

## Note Tecniche

### Timeshift Manager

Per stream HTTP, il sistema usa **TimeshiftManager** che:
- Buffer circolare in PSRAM o SD card
- Seek all'interno del buffer disponibile
- Auto-pause quando buffer insufficiente
- Durata buffer dipende dal bitrate stream

### Formati Supportati

**Audio Codecs:**
- MP3
- WAV
- AAC (stream)

**Sorgenti:**
- HTTP/HTTPS streaming
- SD Card (SD_MMC)
- LittleFS (flash interna)

### Metadata ID3

I metadata (Title, Artist, Album) vengono estratti automaticamente da:
- Tag ID3 nei file MP3
- ICY metadata negli stream HTTP (se disponibile)

### Gestione Errori

Tutte le funzioni ritornano sempre:
1. `success` (boolean) - indica se l'operazione è riuscita
2. `message` (string) - messaggio descrittivo

**Esempio gestione errori:**

```lua
local ok, msg = radio.play("invalid_url")
if not ok then
    println("ERRORE: " .. msg)
    -- Logica di fallback
end
```

---

## Comando CommandCenter

Oltre all'API Lua, è disponibile il comando diretto:

```bash
# Avvia stream
radio_play http://stream.example.com/radio.mp3

# Avvia file
radio_play /sd/music/song.mp3

# Mostra status (senza argomenti)
radio_play
```

**Output status comando:**
```
State: PLAYING
Volume: 75%
Source: HTTP_STREAM (Timeshift)
Title: Jazz Classics
Position: 45s / 180s (buffered)
```

---

## Integrazione AI/Voice Assistant

L'AI può usare queste API per:

1. **Avviare radio**: `radio.play("http://...")`
2. **Controllare stato**: `radio.status()` per sapere cosa è in riproduzione
3. **Pausa/Resume**: Controllare la riproduzione
4. **Seek**: Navigare nel buffer timeshift
5. **Volume**: Regolare il volume dinamicamente

**Esempio prompt AI:**

> "Avvia Radio Paradise e dimmi cosa sta suonando dopo 10 secondi"

```lua
radio.play("http://stream.radioparadise.com/aac-320")
delay(10000)
local ok, status = radio.status()
println(status)
```

---

## Troubleshooting

### Stream non si avvia

```lua
-- Verifica connessione WiFi
local ok, heap = system.heap()
println("Heap disponibile: " .. heap)

-- Verifica URL
radio.play("http://stream.example.com/test.mp3")
delay(5000)
local ok, status = radio.status()
println(status)  -- Controlla se lo stato è ERROR
```

### Seek non funziona

Il seek funziona solo se:
- Per file locali: il file è completamente accessibile
- Per stream HTTP: c'è buffer timeshift disponibile

```lua
local ok, status = radio.status()
-- Controlla se "Timeshift: Available" è presente
```

### Buffer underrun

Se lo stream si interrompe frequentemente:

```lua
-- Aumenta il buffer di pre-caricamento aspettando più tempo
radio.play("http://stream.example.com/radio.mp3")
delay(15000)  -- Aspetta 15 secondi prima di fare altre operazioni
```

---

## Changelog API

### v1.0 (2025-12-05)
- ✅ Implementazione iniziale completa
- ✅ 7 funzioni radio in namespace `radio`
- ✅ Supporto HTTP streaming + timeshift
- ✅ Supporto file locali SD/LittleFS
- ✅ Metadata ID3 automatico
- ✅ Comando `radio_play` aggiornato nel CommandCenter
