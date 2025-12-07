# Comportamento del Microfono e File Generati

## 📋 Overview
Questo documento descrive il comportamento del microfono integrato ES8311, il formato dei file audio generati e le considerazioni per l'implementazione di activation word e pipeline di invio a Whisper.

## 🎤 Configurazione Hardware del Microfono

### ES8311 Codec Integration
Il microfono utilizza il codec ES8311 configurato tramite `es8311_microphone_config()`:
- **Sample Rate**: 16000 Hz (ottimizzato per speech recognition)
- **Bit Depth**: 16-bit PCM
- **Channels**: Mono (1 canale)
- **I2S Port**: I2S_NUM_1 (separato dall'output audio)
- **Pinning**:
  - BCLK: GPIO 5
  - WS (LRCLK): GPIO 7
  - DIN: GPIO 6
  - MCLK: GPIO 4 (APLL per timing preciso)

### Ottimizzazioni Mono/Stereo nell'Audio Player
L'`audio_player.cpp` è stato ottimizzato per gestire correttamente input mono:
- **Tracking canali input**: `input_channels` traccia i canali del file sorgente
- **Upmix condizionale**: Solo file mono vengono duplicati a stereo per output
- **Logging decisioni**: Log espliciti per decisioni di upmix ("Mono source detected, duplicating to stereo")
- **Buffer allocation**: Buffer PCM allocati per `output_channels` effettivi
- **Duplicazione in-place**: Per mono, campioni duplicati direttamente nel buffer prima dell'I2S write
- **Seek fallback**: Dimensioni discard calcolate su `input_channels` decodificati

## 📁 Formato dei File Audio Generati

### WAV Header Structure
I file registrati utilizzano formato WAV standard con header customizzato:

```cpp
struct WAVHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t fileSize = 0;                    // Aggiornato post-registrazione
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t formatLength = 16;
    uint16_t formatType = 1;                  // PCM
    uint16_t channels = 1;                    // Mono
    uint32_t sampleRate = 16000;              // 16kHz per Whisper
    uint32_t bytesPerSecond = 32000;          // sampleRate * channels * bits/8
    uint16_t blockAlign = 2;                  // channels * bits/8
    uint16_t bitsPerSample = 16;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize = 0;                    // Aggiornato post-registrazione
};
```

### Caratteristiche dei File Registrati
- **Formato**: WAV PCM mono 16-bit
- **Sample Rate**: 16000 Hz (compatibile con Whisper)
- **Durata**: Default 6 secondi, configurabile
- **Naming**: `test_NNNNNN.wav` (numero sequenziale a 6 cifre)
- **Storage**: SD card preferita, fallback su LittleFS
- **Directory**: `/test_recordings/` su filesystem selezionato

### Automatic Gain Control (AGC)
Durante la registrazione viene applicato AGC dinamico:
- **Target Peak**: 32000 (su range 16-bit)
- **Gain Factor Max**: 20x
- **Applicazione**: Post-capture, pre-WAV write
- **Overflow Protection**: Clamping a INT16_MIN/MAX

## 🔄 Pipeline di Elaborazione Audio

### Voice Activity Detection (VAD)
Implementazione semplice ma efficace:
- **Soglia**: 800 (su range 16-bit, ~2.4% di max)
- **Timeout Silenzio**: 2 secondi
- **Buffer**: 32KB PSRAM per 2 secondi a 16kHz mono
- **Chunk Size**: 1024 campioni per analisi VAD

### Buffer Management
- **PSRAM Allocation**: 32768 byte per buffer input
- **Queue System**: AudioQueue per passaggio tra task
- **Sample Rate**: Costante 16kHz per compatibilità Whisper
- **Mono Processing**: Tutto l'audio mantenuto mono fino all'output finale

## 🎯 Considerazioni per Activation Word

### Formato Ottimale per Wake Word Detection
- **Mono**: Rilevamento semplificato senza cancellazione eco stereo
- **16kHz**: Standard per speech recognition models
- **16-bit**: Precisione sufficiente per VAD e wake word
- **WAV**: Header standard, facilmente parsabile

### Pipeline Whisper Integration
```cpp
// Esempio processamento per Whisper
AudioBuffer buffer = {
    .data = malloc(samples * sizeof(int16_t)),
    .size = samples * sizeof(int16_t),
    .sample_rate = 16000,
    .bits_per_sample = 16,
    .channels = 1
};

// 1. Capture da I2S → buffer circolare
// 2. VAD detection → trigger recording
// 3. Buffer → WAV file con header
// 4. File → base64 encoding per HTTP
// 5. POST a /v1/audio/transcriptions
// 6. Risposta JSON → parsing comandi
```

### Ottimizzazioni per Latency
- **Pre-allocation**: Buffer PSRAM sempre disponibili
- **Direct I2S**: Nessuna conversione intermedia
- **Mono-only**: Riduce overhead di processamento
- **Chunked Processing**: Analisi VAD su piccoli chunk

## 🛠️ Testing e Troubleshooting

### Microphone Test Screen
Interfaccia completa per validazione:
- **Recording**: 6 secondi con AGC e level indicator
- **Playback**: Test immediato dei file registrati
- **File Management**: Lista e selezione file salvati
- **Visual Feedback**: Arc indicator per livello microfono

### Debug Information
Log dettagliati per troubleshooting:
- **I2S Setup**: Conferma configurazione codec
- **Buffer Status**: Utilizzo memoria e timing
- **VAD Events**: Trigger e timeout silenzi
- **File I/O**: Success/failure scritture su storage

### Performance Metrics
- **CPU Usage**: ~5-10% durante registrazione attiva
- **Memory**: 32KB PSRAM + heap per WAV writing
- **Storage**: ~192KB per minuto di audio (16kHz mono 16-bit)
- **Latency**: <100ms da audio a buffer pronto

## 🚀 Raccomandazioni per Implementazione

### Per Activation Word
1. **Pre-record Buffer**: Mantieni 1-2 secondi di audio precedente per context
2. **Sliding Window VAD**: Analizza continuamente senza gap
3. **Keyword Spotting**: Integra Snowboy/PocketSphinx per wake word
4. **Buffer Circular**: Evita copia memoria durante recording

### Per Whisper Pipeline
1. **Chunking**: Suddividi audio lunghi in segmenti da 25MB max
2. **Compression**: Considera MP3 encoding per ridurre bandwidth
3. **Caching**: Salva transcriptions per evitare re-processing
4. **Error Recovery**: Retry con backoff per API failures

### Ottimizzazioni Future
- **VAD Avanzato**: SNR-based invece che threshold semplice
- **Wake Word Embedding**: Modelli locali per always-on listening
- **Audio Preprocessing**: Noise reduction e echo cancellation
- **Multi-channel**: Supporto array microfoni per beamforming
