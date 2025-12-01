# 🚀 Roadmap: Implementazione Push-to-Talk Voice Assistant

## 🎯 **Obiettivo Finale**
Sistema voice assistant con pulsante hold-to-talk: premi per registrare, rilascia per elaborare comando vocale.

## 📋 **Fasi di Sviluppo**

### **Fase 1: UI e Controllo Base** ✅ COMPLETATA
- [x] **Modifica pulsante UI** per eventi PRESSED/RELEASED
- [x] **Feedback visivo** durante registrazione (pulsante rosso)
- [x] **Metodi VoiceAssistant**: `startRecording()`, `stopRecordingAndProcess()`

### **Fase 2: Voice Capture Engine** ✅ FUNZIONANTE
- [x] **Modifica VoiceCaptureTask** per controllo start/stop invece di timeout
- [x] **Buffer management** per accumulo audio durante pressione
- [x] **Integrazione ES8311** microfono (es8311_microphone_config)
- [x] **Fix I2S input** - Risolto conflitto I2S_NUM_1 → I2S_NUM_0 ✅

### **Fase 3: OpenAI Integration**
- [ ] **STTTask (Whisper)**: Conversione audio→testo
- [ ] **AIProcessingTask (GPT)**: Elaborazione comandi con prompt strutturato
- [ ] **HTTP client** per API calls con retry/backoff

### **Fase 4: Command Execution**
- [ ] **JSON parsing** risposte GPT con cJSON
- [ ] **Command dispatch** al CommandCenter esistente
- [ ] **Feedback utente** post-esecuzione

### **Fase 5: Testing & Polish**
- [ ] **API key validation** nelle settings
- [ ] **Error handling** per WiFi/API failures
- [ ] **Performance optimization** e memory management

## 🎨 **UX Flow**
1. 👆 **Hold "Hold to Talk"** → Pulsante diventa rosso + microfono attivo
2. 🗣️ **Parla comando** → Audio buffer accumula
3. 👆 **Release pulsante** → Pulsante torna verde + "Processing..."
4. 🤖 **STT → GPT → Command** → Esecuzione automatica
5. ✅ **Feedback** → Mostra comando eseguito

## 🔧 **Architettura Tecnica**
```
UI Button (PRESSED) → VoiceAssistant::startRecording()
    ↓
VoiceCaptureTask (attivo) → Accumula audio in buffer
    ↓
UI Button (RELEASED) → VoiceAssistant::stopRecordingAndProcess()
    ↓
Buffer → STTTask (Whisper) → Text → AIProcessingTask (GPT) → Command → CommandCenter
```

## ⚡ **Dipendenze**
- ESP-IDF HTTP client (disponibile)
- cJSON library (da aggiungere se mancante)
- openESPaudio per ES8311 (già integrato)

## 🚨 **PROBLEMA RISOLTO**

### **Sintomi dal Test (PRE-FIX)**
- ✅ UI pulsante funziona: `startRecording()` → `ES8311 microphone enabled`
- ✅ ES8311 si abilita correttamente
- ❌ **Sempre "No audio data recorded"** - VoiceCaptureTask non accumula audio
- ✅ Microphone test normale funziona (61440 bytes registrati)

### **Diagnosi e Fix**
Il VoiceCaptureTask non riceve dati dall'I2S quando `recording_active_` è true.
**Causa identificata**: **Conflitto I2S_NUM_1** - La stessa porta I2S era usata sia per audio input che output.

**Soluzioni implementate**:
1. ✅ **Cambiato I2S port** da `I2S_NUM_1` a `I2S_NUM_0` per evitare conflitto
2. ✅ **Aggiunto logging dettagliato** in VoiceCaptureTask per debug futuro
3. ✅ **Aggiornati commenti** per riflettere la separazione I2S

### **Test Comparativo**
Microphone test normale: ✅ 61440 bytes in 1959ms
Voice Assistant: ❌ 0 bytes (sempre)

## 📋 **Prossimi Passi per il Fix**

### **Fase 2.1: Debug I2S** 🔧 URGENTE
- [ ] **Verificare conflitto I2S_NUM_1** con audio output
- [ ] **Testare I2S config** in isolamento (senza audio output)
- [ ] **Aggiungere debug logging** dettagliato in VoiceCaptureTask
- [ ] **Controllare errori I2S** (esp_err_t return codes)

### **Fase 2.2: Alternative I2S**
- [ ] **Usare I2S_NUM_0** se disponibile
- [ ] **Modificare config** per coesistenza con audio output
- [ ] **Testare shared I2S** approccio se necessario

### **Fase 2.3: Buffer Debug**
- [ ] **Aggiungere logging** di bytes_read in ogni ciclo
- [ ] **Testare buffer PSRAM** allocation e access
- [ ] **Verificare timing** del task

## 📊 **Milestones**
- **MVP**: Pulsante hold-to-talk funzionante con registrazione ✅ UI OK
- **MVP2**: VoiceCaptureTask riceve audio dall'I2S ✅ I2S CONFLICT FIXED
- **Beta**: STT funzionante con comandi base
- **Release**: Full GPT integration con tutti i comandi

## 🎯 **Priorità Implementazione**
1. **Fase 3 (OpenAI)** - STT e GPT integration 🔄 NEXT
2. **Fase 4 (Commands)** - Command execution e feedback
3. **Fase 5 (Polish)** - Testing e ottimizzazioni
