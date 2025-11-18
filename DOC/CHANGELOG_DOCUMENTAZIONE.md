# Changelog Documentazione - OS ESP32 S3 Display Touch

## [2.2.0] - 2025-11-18

### 📊 Aggiornamento Stato Progetto

- **STATO_ATTUALE_PROGETTO.md** - Aggiornamento maggiore stato reale implementazione
  - Versione software aggiornata: 0.1.0 → 0.5.0
  - Stato: Fase Test Hardware → Fase Core Managers Implementati
  - Documenta architettura OS realmente implementata:
    - ✅ LVGL 8.4.0 integrato e funzionante
    - ✅ Touch FT6336U operativo
    - ✅ Core Managers (AppManager, ScreenManager, EventRouter)
    - ✅ Widget System (SystemInfoWidget, ClockWidget, DashboardWidget, DockWidget)
    - ✅ Dashboard Screen funzionante
    - ✅ Settings/Info Screen create (placeholder)
    - ✅ FreeRTOS task LVGL su Core 1
    - ✅ Thread-safety con LVGL mutex
    - ✅ Memory management PSRAM-aware

  - Aggiornata struttura main.cpp: da 130 righe test → 228 righe OS completo
  - Nuova sezione "Funzionalità Implementate" dettagliata con riferimenti file
  - Aggiornata sezione "Cosa NON Implementato":
    - Service Layer (WiFi, BLE, NTP, MQTT)
    - SettingsManager con NVS
    - Peripheral Manager Layer
    - Ottimizzazioni PSRAM (heap LVGL in DRAM, no double buffer)
    - Widget avanzati (WiFiStatus, Battery, Chart)
    - File System (SPIFFS/LittleFS)
    - Theme System

  - Roadmap completamente rivista con stato reale:
    - ~~Fase 1-3, 5, 7~~ marcate ✅ COMPLETATA
    - Fase 8B: Ottimizzazione PSRAM aggiunta come 🔥 PRIORITÀ ALTA
    - Ogni fase con checklist dettagliata e status

  - Tempi aggiornati:
    - Stato attuale: ~50% completato
    - Tempo rimanente: 11-24 giorni (2-5 settimane)
    - Core features prioritarie: 11-12 giorni
    - Features opzionali: +8-12 giorni

  - Milestone riviste:
    - ~~Milestone 1-2~~ marcate ✅ RAGGIUNTA
    - Milestone 3: "Ottimizzazione PSRAM" - 🔥 IN CORSO
    - Milestone 4: "Servizi Connessi" - 🎯 PROSSIMA
    - Milestone 5: "Production-Ready" - 📋 FUTURO

### 🎯 Highlights Stato Progetto

**Completato (~50%):**
- Sistema operativo base funzionante
- Architettura core implementata
- UI interattiva con touch
- Widget system operativo
- Dashboard navigabile

**In Corso:**
- Ottimizzazione PSRAM (priorità alta)
- Settings/Info Screen (UI da completare)

**Prossimi Passi:**
1. Ottimizzazione PSRAM (2-3 giorni) - Liberare +125% DRAM
2. SettingsManager + NVS (1-2 giorni)
3. WiFiService (2-3 giorni)
4. Testing (2-3 giorni)

**Riferimenti:**
- [STATO_ATTUALE_PROGETTO.md](STATO_ATTUALE_PROGETTO.md) - Status reale dettagliato
- [OTTIMIZZAZIONE_PSRAM_LVGL.md](OTTIMIZZAZIONE_PSRAM_LVGL.md) - Piano PSRAM prioritario

---

## [2.1.0] - 2025-11-18

### ✨ Nuovi Documenti

- **OTTIMIZZAZIONE_PSRAM_LVGL.md** - Strategia completa ottimizzazione PSRAM
  - Piano dettagliato custom allocator LVGL in PSRAM (512KB)
  - Double buffering 30 righe in PSRAM (vs 20 righe DRAM)
  - Wrapper `ui_alloc()` intelligente con fallback DRAM
  - Memory monitoring dashboard real-time (DRAM/PSRAM/LVGL heap)
  - Troubleshooting 4 problemi comuni + soluzioni
  - 6 fasi implementazione con codice completo
  - Roadmap 3 settimane
  - Appendici: riferimenti tecnici + sprite pool

### 📝 Modifiche Documenti Esistenti

- **INDICE_DOCUMENTAZIONE.md**
  - Aggiunta sezione OTTIMIZZAZIONE_PSRAM_LVGL.md
  - Descrizione dettagliata caratteristiche chiave
  - Tabelle benefici attesi (DRAM +125%, LVGL heap 2x, buffer 3x)
  - File da creare/modificare per implementazione

### 🎯 Contenuti Aggiunti

**Memory Management:**
- Custom allocator LVGL con `LV_MEM_CUSTOM=1`
- Funzioni `lvgl_malloc/free/realloc` con priorità PSRAM
- Wrapper `ui_alloc(size, label)` per oggetti UI pesanti
- Wrapper `sprite_alloc(size)` per TFT_eSPI sprite pool
- Funzione `lvgl_heap_monitor_print()` per diagnostica

**Dashboard Monitoring:**
- Widget `SystemInfoWidget` esteso con 3 nuove label:
  - PSRAM free/total con colori dinamici
  - LVGL heap usage con percentuale
  - Warning frammentazione se >20%
- Refresh rate aumentato a 1 secondo (da 2s)

**Testing & Validation:**
- Test stress allocazione (32 blocchi x 64KB = 2MB)
- Monitoring periodico ogni 60s
- Memory leak detection pattern
- Profiling frame rate

**Troubleshooting:**
1. "PSRAM not detected" → board config + test manuale
2. "lvgl_malloc allocation failed" → debug frammentazione
3. "UI freeze dopo un po'" → memory leak detection
4. "Double buffering lento" → tuning buffer size

**Esempi Codice:**
- Setup completo `main.cpp` con custom allocator
- Widget `HistoryPlotWidget` con buffer PSRAM (1000 sample)
- Sprite pool completo per animazioni
- Test stress allocazione con verifica integrità

### 📊 Statistiche

**Righe di Codice Documentate:**
- `psram_allocator.h`: ~80 righe (API completa)
- `psram_allocator.cpp`: ~200 righe (implementazione)
- `system_info_widget.cpp` modifiche: ~60 righe
- Esempi completi: ~150 righe
- **Totale nuovo codice:** ~490 righe

**Dimensione Documento:**
- OTTIMIZZAZIONE_PSRAM_LVGL.md: ~1200 righe
- 12 sezioni principali
- 6 fasi implementazione dettagliate
- 2 appendici tecniche
- 4 problemi troubleshooting
- 3 esempi completi ready-to-use

### 🔗 Collegamenti

**Documenti Correlati:**
- [ESTENSIONI_ARCHITETTURA_SERVIZI.md](ESTENSIONI_ARCHITETTURA_SERVIZI.md) - Sezione 1.3 Memory Management
- [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) - Sezione 12.1 Memory Management
- [STATO_ATTUALE_PROGETTO.md](STATO_ATTUALE_PROGETTO.md) - Hardware specs ESP32-S3

**Riferimenti Esterni:**
- ESP32-S3 Technical Reference Manual - Sezione PSRAM
- LVGL v8.4 Custom Memory Documentation
- ESP-IDF Heap Capabilities API

---

## [2.0.0] - 2025-11-14

### ✨ Aggiunte Principali

#### Nuovi Documenti

1. **[REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md)** - NUOVO! ⭐
   - Unificazione di REPORT_ARCHITETTURA_OS_ESP32.md e ESTENSIONI_ARCHITETTURA_SERVIZI.md
   - Architettura completa a 5 layer
   - Tutti i componenti con codice completo e ready-to-use
   - 17 sezioni dettagliate (da Stack Tecnologico a Risorse)
   - Thread-safety patterns approfonditi
   - FreeRTOS task map con core pinning
   - Problemi comuni e soluzioni dalla community
   - Checklist pre-deployment completa

2. **[ESTENSIONE_PERIPHERAL_MANAGER.md](ESTENSIONE_PERIPHERAL_MANAGER.md)** - NUOVO! ⭐
   - Peripheral Manager Layer per gestione hardware
   - GPIO Manager completo (allocazione, interrupt, ownership)
   - I2C Manager completo (multi-bus, scan, hot-plug)
   - Pattern per SPI/ADC/PWM/UART Manager
   - Resource management e prevenzione conflitti
   - API thread-safe per tutte le periferiche
   - Esempi di integrazione con Services e Apps

3. **[README_DOCUMENTAZIONE.md](README_DOCUMENTAZIONE.md)** - NUOVO!
   - Guida completa alla navigazione dei documenti
   - Mappa della documentazione
   - Percorsi di lettura consigliati
   - Checklist implementazione passo-passo
   - Quick start guide
   - Configurazione PlatformIO ready-to-use

#### Nuove Funzionalità Architetturali

**Peripheral Manager Layer** (completamente nuovo!)
```
┌─────────────────────────────────────────────────────────────────┐
│              PERIPHERAL MANAGER LAYER                            │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ PeripheralManager (Resource Registry)                       │ │
│  │  GPIO | I2C | SPI | ADC | PWM | UART                        │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

**Caratteristiche:**
- ✅ Allocazione/deallocazione risorse hardware
- ✅ Ownership tracking (chi sta usando GPIO 5?)
- ✅ Conflict detection automatica
- ✅ Thread-safe operations con mutex
- ✅ Hot-plug detection per I2C devices
- ✅ Event publishing per eventi hardware

---

### 🔄 Modifiche ai Documenti Esistenti

#### [REPORT_ARCHITETTURA_OS_ESP32.md](REPORT_ARCHITETTURA_OS_ESP32.md)
**Status:** ⛔ DEPRECATO

- ➕ Aggiunto banner di deprecazione in testa al documento
- ➕ Aggiunto link a REPORT_COMPLETO_OS_ESP32.md
- ➕ Aggiunto link a ESTENSIONE_PERIPHERAL_MANAGER.md
- ℹ️ Contenuto originale mantenuto per riferimento storico
- ⚠️ Marcato come "NON USARE PER IMPLEMENTAZIONE"

**Motivo deprecazione:**
- Contenuti unificati in REPORT_COMPLETO_OS_ESP32.md
- Architettura estesa con Peripheral Manager
- Esempi di codice migliorati e completi

#### [ESTENSIONI_ARCHITETTURA_SERVIZI.md](ESTENSIONI_ARCHITETTURA_SERVIZI.md)
**Status:** ⛔ DEPRECATO

- ➕ Aggiunto banner di deprecazione in testa al documento
- ➕ Aggiunto link a REPORT_COMPLETO_OS_ESP32.md
- ➕ Aggiunto link a ESTENSIONE_PERIPHERAL_MANAGER.md
- ℹ️ Contenuto originale mantenuto per riferimento storico
- ⚠️ Marcato come "NON USARE PER IMPLEMENTAZIONE"

**Motivo deprecazione:**
- Service Layer integrato in REPORT_COMPLETO_OS_ESP32.md
- Widget System unificato con architettura completa
- Thread-safety patterns migliorati

---

### 📚 Struttura Documentazione Attuale

```
Documentazione/
│
├── 📘 REPORT_COMPLETO_OS_ESP32.md ⭐ PRINCIPALE
│   └── Architettura completa + Service Layer + Widget System
│
├── 📗 ESTENSIONE_PERIPHERAL_MANAGER.md ⭐ HARDWARE
│   └── GPIO + I2C + Pattern per altre periferiche
│
├── 📖 README_DOCUMENTAZIONE.md ⭐ GUIDA
│   └── Navigazione + Quick Start + Checklist
│
├── 📝 CHANGELOG_DOCUMENTAZIONE.md
│   └── Questo file
│
└── 📂 Riferimenti Storici (DEPRECATI)
    ├── REPORT_ARCHITETTURA_OS_ESP32.md (v1.0)
    └── ESTENSIONI_ARCHITETTURA_SERVIZI.md (v1.0)
```

---

### 📊 Confronto Versioni

#### Versione 1.0 (Pre-unificazione)

**Documenti:**
- REPORT_ARCHITETTURA_OS_ESP32.md (documento base)
- ESTENSIONI_ARCHITETTURA_SERVIZI.md (estensioni)

**Architettura:**
- 4 layer (Hardware → HAL → GUI → Managers → Services → Apps)
- Service Layer per WiFi/BLE
- Widget System per dashboard
- Thread-safety base

**Problemi:**
- ❌ Documentazione frammentata (2 documenti separati)
- ❌ Nessuna gestione periferiche hardware
- ❌ Esempi di codice incompleti
- ❌ Difficile navigazione per utenti nuovi

---

#### Versione 2.0 (Attuale)

**Documenti:**
- REPORT_COMPLETO_OS_ESP32.md (principale, unificato)
- ESTENSIONE_PERIPHERAL_MANAGER.md (hardware)
- README_DOCUMENTAZIONE.md (guida)

**Architettura:**
- 5 layer (aggiunto Peripheral Manager Layer)
- Service Layer completo con esempi
- Widget System completo con esempi
- **Peripheral Manager** per GPIO/I2C/SPI/ADC/PWM/UART
- Thread-safety avanzata
- Resource management

**Miglioramenti:**
- ✅ Documentazione unificata e coerente
- ✅ Gestione periferiche hardware completa
- ✅ Esempi di codice ready-to-use
- ✅ Guida navigazione per utenti nuovi
- ✅ Checklist implementazione step-by-step
- ✅ Problemi comuni e soluzioni dalla community

---

### 🆕 Nuove Sezioni nel Report Completo

| Sezione | Contenuto | Novità |
|---------|-----------|---------|
| 1. Stack Tecnologico | LVGL, TFT_eSPI, FT6336U, hardware | Migliorato |
| 2. Architettura Software | 5 layer + diagrammi + regole critiche | **Esteso** |
| 3. Componenti Core | Screen/App/Settings Manager + EventRouter | Migliorato |
| 4. Service Layer | WiFi/BLE Service + ServiceManager | Migliorato |
| 5. Widget System | WiFi/System/Clock widget completi | **Completo** |
| 6. Thread-Safety | Pattern mutex + regole d'oro + esempi | **Approfondito** |
| 7. Inizializzazione | Setup completo con ordine critico | **Nuovo** |
| 8. Configurazione PlatformIO | platformio.ini ready-to-use | **Nuovo** |
| 9. Struttura File System | Organizzazione completa progetto | **Nuovo** |
| 10. FreeRTOS Task Map | Diagramma core + priorità | **Nuovo** |
| 11. Problemi Comuni | UI freeze, WiFi/BLE lento, memory leak | **Nuovo** |
| 12. Best Practices | Memory, MVC, error handling | **Nuovo** |
| 13. Checklist Pre-Deployment | Verifica completa pre-deploy | **Nuovo** |
| 14. Vantaggi Architettura | Punti di forza | **Nuovo** |
| 15. Limitazioni | Considerazioni importanti | **Nuovo** |
| 16. Prossimi Passi | Roadmap implementazione | **Nuovo** |
| 17. Risorse | Links documentazione + community | **Nuovo** |

---

### 🛠️ Nuove API Periferiche

#### GPIO Manager

```cpp
// Alloca GPIO con ownership tracking
GPIOPeripheral* led = gpio_mgr->requestGPIO(4, GPIO_MODE_OUTPUT, "MyApp");

// Operazioni
led->write(HIGH);
led->toggle();
bool state = led->read();

// Interrupt
led->attachInterrupt([]() {
    Serial.println("Button pressed!");
}, FALLING);

// Rilascio
gpio_mgr->releaseGPIO(4, "MyApp");
```

#### I2C Manager

```cpp
// Inizializza bus I2C
I2CPeripheral* i2c0 = i2c_mgr->initBus(0, 21, 22, 100000);

// Scan devices
i2c0->scanBus();

// Alloca device
i2c0->allocateDevice(0x48, "SensorService", "TMP102");

// Operazioni thread-safe
i2c0->writeRegister(0x48, 0x01, 0xFF);
uint8_t value;
i2c0->readRegister(0x48, 0x00, &value);

// Deallocazione
i2c0->deallocateDevice(0x48, "SensorService");
```

---

### 🎯 Migrazioni Necessarie

#### Se usavi Versione 1.0:

**Da fare:**

1. **Aggiorna riferimenti documentazione:**
   - ❌ REPORT_ARCHITETTURA_OS_ESP32.md
   - ❌ ESTENSIONI_ARCHITETTURA_SERVIZI.md
   - ✅ REPORT_COMPLETO_OS_ESP32.md

2. **Aggiungi Peripheral Manager (se usi GPIO/I2C):**
   ```cpp
   // Inizializza Peripheral Manager
   auto periph_mgr = PeripheralManager::getInstance();
   periph_mgr->init();

   // Usa GPIO Manager
   auto gpio_mgr = periph_mgr->getGPIOManager();
   GPIOPeripheral* led = gpio_mgr->requestGPIO(2, GPIO_MODE_OUTPUT, "system");
   ```

3. **Aggiorna inizializzazione sistema:**
   - Segui sezione 7 di REPORT_COMPLETO_OS_ESP32.md
   - Ordine critico: Hardware → LVGL → Mutex → Tick → FS → Managers → **Periferiche** → Services → Apps → LVGL task

4. **Verifica thread-safety:**
   - Leggi sezione 6 di REPORT_COMPLETO_OS_ESP32.md
   - Assicurati che tutti gli update UI usino mutex LVGL

**Compatibilità:**
- ✅ Core Managers (Screen, App, Settings, EventRouter) - API invariata
- ✅ Service Base Class - API invariata
- ✅ Widget Base Class - API estesa (aggiunto `safeUpdateUI()`)
- ⚠️ GPIO/I2C - Ora gestiti da Peripheral Manager (migrazione necessaria)

---

### 📈 Statistiche

| Metrica | v1.0 | v2.0 | Differenza |
|---------|------|------|------------|
| **Documenti principali** | 2 | 3 | +1 |
| **Layer architettura** | 4 | 5 | +1 (Peripheral Manager) |
| **Sezioni totali** | 10 | 17 | +7 |
| **Esempi codice completi** | 8 | 25+ | +17 |
| **Manager implementati** | 4 | 7+ | +3 (GPIO, I2C, Peripheral) |
| **Widget pronti** | 0 | 3 | +3 (WiFi, System, Clock) |
| **Righe codice esempi** | ~800 | ~2500 | +1700 |
| **Problemi comuni risolti** | 3 | 10+ | +7 |

---

### 🔮 Prossime Versioni (Roadmap)

#### v2.1 (Pianificato)

- [ ] Implementare SPI Manager completo
- [ ] Implementare ADC Manager completo
- [ ] Implementare PWM Manager (LEDC) completo
- [ ] Implementare UART Manager completo
- [ ] Aggiungere esempi app complete (Weather, LED Controller, Sensor Dashboard)

#### v2.2 (Pianificato)

- [ ] NTP Service completo con timezone
- [ ] MQTT Service per IoT
- [ ] HTTP Server per API REST
- [ ] OTA Update system
- [ ] File Manager per SD card

#### v3.0 (Futuro)

- [ ] App Store interno per installare app dinamicamente
- [ ] Plugin system per estensioni
- [ ] Remote debugging via WiFi
- [ ] Multi-language support
- [ ] Theme system per UI customization

---

### ⚠️ Breaking Changes

Nessun breaking change nell'API core. Tutti i componenti della v1.0 sono compatibili.

**Nuove funzionalità (opt-in):**
- Peripheral Manager (opzionale, ma raccomandato per progetti con GPIO/I2C)

---

### 🐛 Bug Fix

- ✅ Corretto esempio WiFiService: aggiunto event publishing su tutti gli stati
- ✅ Corretto esempio mutex LVGL: aggiunto timeout per evitare deadlock
- ✅ Corretto esempio Widget: `safeUpdateUI()` ora gestisce correttamente mutex
- ✅ Aggiornato FreeRTOS task map: priorità verificate con community ESP32

---

### 📝 Documentazione Migliorata

- ✅ Aggiunto diagramma architettura a 5 layer
- ✅ Aggiunto FreeRTOS task map dettagliato
- ✅ Aggiunte sezioni "Problemi Comuni" con soluzioni verificate
- ✅ Aggiunta checklist pre-deployment completa
- ✅ Aggiunti percorsi di lettura consigliati per diversi use-case
- ✅ Aggiunta configurazione PlatformIO ready-to-use
- ✅ Aggiunte note su licenze (MIT, BSD, LGPL)

---

### 👥 Contributors

- **ESP32 OS Architecture Team**
- **Community ESP32 Forum** (soluzioni problemi comuni)
- **Community LVGL Forum** (thread-safety patterns)
- **Tactility OS Project** (ispirazione Service Layer)

---

### 📄 Licenze

Documentazione: CC BY 4.0
Esempi di codice: MIT License
Librerie utilizzate:
- LVGL: MIT License
- TFT_eSPI: FreeBSD License
- ESP32 Arduino: LGPL 2.1

---

**Versione:** 2.0.0
**Data:** 2025-11-14
**Tipo Release:** Major (nuova architettura Peripheral Manager)

---

## Come Leggere Questo Changelog

- ✨ **Aggiunte**: Nuove funzionalità
- 🔄 **Modifiche**: Cambiamenti a funzionalità esistenti
- 🐛 **Bug Fix**: Correzioni
- ⛔ **Deprecazioni**: Funzionalità deprecate
- ⚠️ **Breaking Changes**: Modifiche che richiedono migrazione
- 📝 **Documentazione**: Miglioramenti documentazione

---

**Per domande o supporto, consulta [README_DOCUMENTAZIONE.md](README_DOCUMENTAZIONE.md).**
