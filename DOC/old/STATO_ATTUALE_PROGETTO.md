# Stato Attuale del Progetto ESP32-S3 Touch Display

**Data:** 2025-11-18
**Versione Software:** 0.5.0 (Fase Core Managers Implementati)
**Board:** Freenove ESP32-S3 Display 2.8"

---

## 1. Specifiche Hardware Reali

### 1.1 Board: Freenove ESP32-S3 Display

**Microcontroller:**
- **Chip:** ESP32-S3-WROOM-1
- **CPU:** Xtensa dual-core 32-bit LX7 @ 240 MHz
- **Flash:** 16MB
- **PSRAM:** Disponibile (flag `-DBOARD_HAS_PSRAM` attivo)
- **Memory Type:** QIO QSPI

**Display TFT:**
- **Controller:** ILI9341
- **Risoluzione:** 240x320 pixel
- **Dimensione:** 2.8 pollici
- **Interfaccia:** SPI (HSPI Port)
- **Orientamento Colore:** BGR
- **Inversione:** ON

**Touch Controller:**
- **Chip:** FT6336U
- **Interfaccia:** I2C
- **Tipo:** Touch capacitivo
- **Multi-touch:** Supportato

### 1.2 Pinout TFT Display (da Freenove_TFT_Config.h)

| Funzione | GPIO | Note |
|----------|------|------|
| **TFT_MISO** | 13 | SPI Master In Slave Out |
| **TFT_MOSI** | 11 | SPI Master Out Slave In |
| **TFT_SCLK** | 12 | SPI Clock |
| **TFT_CS** | 10 | Chip Select |
| **TFT_DC** | 46 | Data/Command |
| **TFT_RST** | -1 | Reset (non usato) |
| **TFT_BL** | 45 | Backlight Control |

**SPI Frequency:** 40 MHz
**SPI Port:** HSPI

### 1.3 Font Caricati

Il sistema TFT_eSPI ha i seguenti font pre-caricati:
- LOAD_GLCD (base font)
- LOAD_FONT2
- LOAD_FONT4
- LOAD_FONT6
- LOAD_FONT7
- LOAD_FONT8
- LOAD_GFXFF (GFX Free Fonts)
- SMOOTH_FONT

---

## 2. Configurazione PlatformIO

### 2.1 File platformio.ini

```ini
[env:freenove-esp32-s3-display]
platform = espressif32
board = esp32-s3-devkitm-1
framework = arduino

board_build.flash_mode = qio
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
board_build.arduino.memory_type = qio_qspi

monitor_speed = 115200
upload_speed = 921600

build_flags =
  -DARDUINO_USB_MODE=1
  -DARDUINO_USB_CDC_ON_BOOT=1
  -DBOARD_HAS_PSRAM
  -mfix-esp32-psram-cache-issue
  -include include/Freenove_TFT_Config.h
```

### 2.2 Build Flags Importanti

| Flag | Descrizione |
|------|-------------|
| **ARDUINO_USB_MODE=1** | Abilita USB in modalità CDC (seriale nativa) |
| **ARDUINO_USB_CDC_ON_BOOT=1** | CDC attivo all'avvio |
| **BOARD_HAS_PSRAM** | Indica la presenza di PSRAM |
| **-mfix-esp32-psram-cache-issue** | Fix per bug cache PSRAM |
| **-include Freenove_TFT_Config.h** | Include automatico configurazione TFT |

---

## 3. Stato Attuale del Software

### 3.1 Fase: Core Managers Implementati + Dashboard Funzionante

Il progetto ha **superato la fase di test hardware** e ha implementato con successo:
- ✅ LVGL 8.4.0 integrato e funzionante
- ✅ Touch FT6336U operativo
- ✅ Core Managers (AppManager, ScreenManager, EventRouter)
- ✅ Widget System (SystemInfoWidget, ClockWidget, DashboardWidget)
- ✅ Dashboard Screen funzionante con widget attivi
- ✅ Settings Screen e Info Screen create
- ✅ Dock widget per navigazione app

### 3.2 Struttura main.cpp Attuale

```
src/main.cpp (228 righe) - Architettura OS Completa
├── Include: Arduino.h, esp headers, lvgl.h, TFT_eSPI.h
├── Core Components:
│   ├── AppManager - Gestione applicazioni
│   ├── TouchDriver - Driver touch FT6336U
│   ├── LVGL Mutex - Thread-safety
│   └── Screens (Dashboard, Settings, Info)
├── Helper Functions:
│   ├── logSystemBanner() - Info hardware all'avvio
│   ├── logMemoryStats() - Monitoring DRAM/PSRAM
│   ├── logLvglBufferInfo() - Info buffer grafico
│   ├── allocatePsramBuffer() - Allocazione PSRAM intelligente
│   ├── enableBacklight() - Controllo retroilluminazione
│   ├── tft_flush_cb() - Callback LVGL → TFT
│   └── lv_tick_handler() - Timer tick LVGL
├── FreeRTOS Tasks:
│   └── lvgl_task() - Task LVGL su Core 1 (priorità 3)
├── setup() - ~100 righe
│   ├── Serial + System banner
│   ├── Memory stats boot
│   ├── PSRAM detection
│   ├── Touch driver init
│   ├── TFT init (rotation 1)
│   ├── LVGL init
│   ├── Draw buffer PSRAM (20 righe, fallback interno)
│   ├── Display driver registration
│   ├── Touch input registration
│   ├── LVGL mutex setup
│   ├── ESP timer tick (1ms)
│   ├── AppManager init + registrazione app
│   ├── Launch dashboard
│   └── Create LVGL task Core 1
└── loop()
    └── vTaskDelay(portMAX_DELAY) - Sistema completamente FreeRTOS-based
```

### 3.3 Funzionalità Attualmente Implementate

✅ **Sistema Operativo Base**
- LVGL 8.4.0 integrato e configurato
- FreeRTOS task management (Core 1 per LVGL)
- Thread-safety con mutex globale LVGL
- Memory management PSRAM-aware

✅ **Display e Touch**
- Controller ILI9341 configurato (320x240)
- Rotazione landscape
- Touch FT6336U I2C funzionante
- Backlight controllato via GPIO 45
- Draw buffer con fallback PSRAM/DRAM

✅ **Core Managers Implementati**
- **AppManager** (src/core/app_manager.h/cpp)
  - Registry applicazioni
  - Dock widget per navigazione
  - Launch/switch app
- **ScreenManager** (src/core/screen_manager.h/cpp)
  - Gestione schermate LVGL
  - Navigazione stack
- **EventRouter** (src/core/event_router.h/cpp)
  - Sistema pub/sub eventi
  - Thread-safe communication

✅ **Widget System**
- **DashboardWidget** base class (src/widgets/dashboard_widget.h/cpp)
- **SystemInfoWidget** (src/widgets/system_info_widget.h/cpp)
  - Heap free monitor
  - Uptime counter
  - Auto-refresh ogni 2s
- **ClockWidget** (src/widgets/clock_widget.h/cpp)
  - Display ora/data
  - Auto-refresh ogni 1s
- **DockWidget** (src/widgets/dock_widget.h/cpp)
  - Barra navigazione app
  - Icone app registrate

✅ **Schermate Implementate**
- **DashboardScreen** (src/screens/dashboard_screen.h/cpp)
  - Layout flex column
  - SystemInfoWidget + ClockWidget integrati
- **SettingsScreen** (src/screens/settings_screen.h/cpp)
- **InfoScreen** (src/screens/info_screen.h/cpp)

✅ **Utilities**
- **LVGL Mutex** (src/utils/lvgl_mutex.h/cpp) - Thread-safety globale
- **Touch Driver** (src/drivers/touch_driver.h/cpp) - FT6336U I2C
- **I2C Scanner** (src/utils/i2c_scanner.h/cpp) - Debug I2C devices

✅ **Memory Management**
- PSRAM detection e logging
- Draw buffer allocation con fallback
- Memory stats monitoring (DRAM/PSRAM)

✅ **Serial Monitor Debug**
- Comunicazione USB CDC @ 115200 baud
- System banner con chip info
- Memory stats dettagliati
- LVGL buffer info

### 3.4 Cosa NON è Ancora Implementato

❌ **Service Layer**
- Nessun WiFi Service
- Nessun BLE Service
- Nessun NTP Service
- Nessun MQTT Service
- Nessun ServiceManager

❌ **Settings Manager**
- Nessuna persistenza NVS
- Nessun SettingsManager
- Settings Screen vuota (solo placeholder)

❌ **Peripheral Manager**
- Nessun GPIO Manager
- Nessun I2C Manager (solo scanner debug)
- Nessun SPI Manager
- Nessun ADC Manager
- Nessun PWM Manager

❌ **Ottimizzazioni PSRAM**
- Heap LVGL ancora in DRAM (256KB)
- Nessun custom allocator LVGL
- Nessun double buffering PSRAM
- Nessun wrapper `ui_alloc()`
- Nessun monitoring PSRAM nel widget

❌ **Widget Avanzati**
- Nessun WiFiStatusWidget
- Nessun BatteryWidget
- Nessun ChartWidget
- Nessun NotificationWidget

❌ **File System**
- Nessun SPIFFS/LittleFS
- Nessuna gestione risorse (icone, font, temi)

❌ **Theme System**
- Nessun sistema temi JSON
- Nessun Asset Manager
- Nessun cambio tema runtime

---

## 4. Librerie Presenti nel Progetto

### 4.1 Directory lib/

```
lib/
├── TFT_eSPI/           # Libreria display TFT (locale)
├── TFT_eSPI_Setups/    # Setup configurations
└── README
```

**Nota:** Al momento è presente SOLO la libreria TFT_eSPI. Non ci sono altre dipendenze installate.

### 4.2 Librerie da Installare per l'OS Completo

Per implementare l'architettura descritta nella documentazione, sarà necessario aggiungere:

| Libreria | Versione Target | Scopo |
|----------|----------------|-------|
| **lvgl** | 8.4.0 | GUI Framework principale |
| **FT6336U** / **CST816S** | Latest | Touch controller driver |
| **ArduinoJson** | 6.x | JSON parsing (Settings/Config) |
| **Preferences** | Built-in ESP32 | NVS Storage wrapper |

---

## 5. Struttura File System Attuale

```
esp-s3-touc-2.8/
├── DOC/                          # Documentazione (aggiornata)
│   ├── INDICE_DOCUMENTAZIONE.md
│   ├── REPORT_COMPLETO_OS_ESP32.md
│   ├── ESTENSIONE_PERIPHERAL_MANAGER.md
│   ├── README_DOCUMENTAZIONE.md
│   ├── CHANGELOG_DOCUMENTAZIONE.md
│   └── STATO_ATTUALE_PROGETTO.md  # <- QUESTO FILE
├── include/
│   └── Freenove_TFT_Config.h     # Configurazione TFT
├── lib/
│   ├── TFT_eSPI/                 # Libreria display
│   └── TFT_eSPI_Setups/
├── src/
│   └── main.cpp                  # Codice test TFT (130 righe)
├── test/                         # Test unitari (vuoto)
├── platformio.ini                # Configurazione build
└── .vscode/                      # Configurazione IDE
```

**Directory da creare per l'OS:**
```
src/
├── core/           # Core Managers (Screen, App, Settings, EventRouter)
├── services/       # Service Layer (WiFi, BLE, NTP, ecc.)
├── widgets/        # Widget System per dashboard
├── apps/           # Applicazioni utente
├── peripherals/    # Peripheral Managers (GPIO, I2C, SPI, ecc.)
├── utils/          # Utilities e helper
└── config/         # File di configurazione
```

---

## 6. Roadmap Implementazione OS Completo

### ~~Fase 1: Setup Base LVGL~~ ✅ **COMPLETATA**
~~**Obiettivo:** Far funzionare LVGL con il display esistente~~

- [x] Installare libreria LVGL 8.4.0
- [x] Configurare lv_conf.h per ESP32-S3
- [x] Creare driver LVGL per TFT_eSPI
- [x] Implementare LVGL tick handler
- [x] Test: Display funzionante con LVGL
- [x] Configurare PSRAM per buffer LVGL (con fallback)

### ~~Fase 2: Touch Controller~~ ✅ **COMPLETATA**
~~**Obiettivo:** Integrare il touch FT6336U~~

- [x] Implementare driver FT6336U I2C
- [x] Configurare I2C per touch controller
- [x] Creare driver LVGL per touch input
- [x] Test: Touch callback e coordinate
- [x] Debug I2C scanner per troubleshooting

### ~~Fase 3: Core Managers~~ ✅ **COMPLETATA**
~~**Obiettivo:** Implementare i manager fondamentali~~

#### ~~EventRouter~~ ✅
- [x] Implementare classe EventRouter
- [x] Sistema pub/sub thread-safe
- [x] File: src/core/event_router.h/cpp

#### ~~ScreenManager~~ ✅
- [x] Implementare classe ScreenManager
- [x] Gestione schermate LVGL
- [x] File: src/core/screen_manager.h/cpp

#### ~~AppManager~~ ✅
- [x] Implementare classe AppManager
- [x] Registry applicazioni con dock
- [x] App lifecycle management
- [x] Registrazione e lancio app
- [x] File: src/core/app_manager.h/cpp

#### SettingsManager ⚠️ **DA COMPLETARE**
- [ ] Implementare classe SettingsManager
- [ ] Persistenza NVS (Preferences)
- [ ] Type-safe getters/setters
- [ ] Callback onChange
- [ ] Test: Salvataggio e caricamento settings
- [ ] Integrare con SettingsScreen

### Fase 4: Service Layer
**Obiettivo:** Creare servizi di background thread-safe

#### Service Base Class
- [ ] Classe base Service astratta
- [ ] Lifecycle hooks (init, start, stop)
- [ ] Core pinning automatico
- [ ] Task wrapper FreeRTOS
- [ ] Test: Service dummy su Core 0

#### WiFiService
- [ ] Estendere Service base
- [ ] State machine WiFi
- [ ] Auto-reconnect logic
- [ ] Event publishing (connected, disconnected)
- [ ] Test: Connessione a rete WiFi

#### BLEService (opzionale)
- [ ] Estendere Service base
- [ ] Advertising management
- [ ] Connection callbacks
- [ ] Test: BLE advertising visibile

#### NTPService (opzionale)
- [ ] Estendere Service base
- [ ] Sync automatico orologio
- [ ] Timezone support
- [ ] Test: Timestamp corretto

#### ServiceManager
- [ ] Registry centralizzato servizi
- [ ] Start/stop all services
- [ ] Status monitoring
- [ ] Test: Gestione multipli servizi

### ~~Fase 5: Widget System Base~~ ✅ **COMPLETATA**
~~**Obiettivo:** Creare widget riutilizzabili per UI~~

#### ~~Widget Base Class~~ ✅
- [x] Classe base DashboardWidget astratta
- [x] Thread-safe update methods
- [x] Layout support
- [x] File: src/widgets/dashboard_widget.h/cpp

#### ~~SystemInfoWidget~~ ✅
- [x] Memory usage (Heap free)
- [x] Uptime counter
- [x] Auto-refresh ogni 2s con timer
- [x] File: src/widgets/system_info_widget.h/cpp

#### ~~ClockWidget~~ ✅
- [x] Time/date display
- [x] Auto-refresh ogni 1s
- [x] File: src/widgets/clock_widget.h/cpp

#### ~~DockWidget~~ ✅
- [x] Barra navigazione app
- [x] Icone app registrate
- [x] File: src/widgets/dock_widget.h/cpp

### Fase 5B: Widget Avanzati ⚠️ **PRIORITÀ MEDIA**
**Obiettivo:** Aggiungere widget specifici per servizi

#### WiFiStatusWidget
- [ ] RSSI signal bar
- [ ] IP address display
- [ ] State colors (connecting/connected/error)
- [ ] Test: Integrazione con WiFiService (da creare)

#### BatteryWidget (opzionale)
- [ ] Voltage indicator
- [ ] Charging status
- [ ] Percentage display

#### ChartWidget (opzionale)
- [ ] Line chart per history dati
- [ ] Buffer circolare in PSRAM
- [ ] Auto-scroll

### Fase 6: Peripheral Manager Layer
**Obiettivo:** Gestione centralizzata periferiche hardware

#### Peripheral Base Class
- [ ] Classe base Peripheral astratta
- [ ] PeripheralType enum
- [ ] PeripheralState enum
- [ ] Allocation/deallocation
- [ ] Test: Peripheral dummy

#### GPIO Manager
- [ ] GPIOPeripheral class
- [ ] GPIOManager registry
- [ ] Request/release GPIO
- [ ] Ownership tracking
- [ ] Interrupt handling
- [ ] Test: Controllo LED

#### I2C Manager
- [ ] I2CPeripheral class
- [ ] Multi-bus support
- [ ] Device scanning
- [ ] Hot-plug detection
- [ ] Test: Scan I2C devices

#### PeripheralManager Registry
- [ ] Centralized manager
- [ ] GPIO/I2C manager getters
- [ ] Status printing
- [ ] Test: Allocation multipli GPIO

### ~~Fase 7: Dashboard Application~~ ✅ **COMPLETATA**
~~**Obiettivo:** Creare la dashboard principale~~

- [x] Implementare DashboardScreen
- [x] Layout flex column per widgets
- [x] Integrare SystemInfoWidget
- [x] Integrare ClockWidget
- [x] File: src/screens/dashboard_screen.h/cpp
- [x] Test: Dashboard funzionante

### Fase 8: Settings & Info Applications ⚠️ **IN PROGRESS**
**Obiettivo:** Completare schermate settings e info

#### SettingsScreen (DA COMPLETARE)
- [x] Creare file base SettingsScreen
- [ ] WiFi SSID/Password input UI
- [ ] Display brightness slider
- [ ] Theme selector (se implementato)
- [ ] About section con versione
- [ ] Integrazione con SettingsManager (da creare)
- [ ] Test: Modifica e salvataggio settings

#### InfoScreen
- [x] Creare file base InfoScreen
- [ ] Mostrare info hardware (chip, flash, PSRAM)
- [ ] Mostrare versione firmware
- [ ] Credits e licenze
- [ ] Test: Display info complete

### Fase 8B: Ottimizzazione PSRAM 🔥 **PRIORITÀ ALTA**
**Obiettivo:** Implementare strategia PSRAM per liberare DRAM

**Riferimento:** [OTTIMIZZAZIONE_PSRAM_LVGL.md](OTTIMIZZAZIONE_PSRAM_LVGL.md)

- [ ] **FASE 1:** Custom allocator LVGL in PSRAM (512KB)
  - [ ] Modificare lv_conf.h → LV_MEM_CUSTOM=1
  - [ ] Creare src/utils/psram_allocator.h/cpp
  - [ ] Implementare lvgl_malloc/free/realloc
  - [ ] Test: LVGL heap in PSRAM
- [ ] **FASE 2:** Double buffering in PSRAM (30 righe)
  - [ ] Modificare main.cpp draw buffer setup
  - [ ] Allocare 2 buffer PSRAM
  - [ ] Test: Rendering fluido
- [ ] **FASE 3:** Wrapper ui_alloc() intelligente
  - [ ] Implementare ui_alloc/ui_free in psram_allocator.cpp
  - [ ] Implementare sprite_alloc per TFT_eSPI
  - [ ] Test: Allocazione widget pesanti
- [ ] **FASE 4:** Memory monitoring dashboard
  - [ ] Estendere SystemInfoWidget con label PSRAM
  - [ ] Aggiungere LVGL heap usage %
  - [ ] Warning frammentazione >20%
  - [ ] Colori dinamici threshold
- [ ] **FASE 5:** Testing e validazione
  - [ ] Test stress allocazione (2MB)
  - [ ] Memory leak detection (run 24h)
  - [ ] Profiling frame rate
  - [ ] Documentazione risultati

**Benefici attesi:**
- DRAM libera: +125% (da 200KB a 450KB)
- LVGL heap: 2x capacità (512KB PSRAM vs 256KB DRAM)
- Draw buffer: 3x dimensione (38KB vs 13KB)

### Fase 9: Testing & Ottimizzazione
**Obiettivo:** Stabilità e performance

- [ ] Memory profiling (leak detection)
- [ ] Core pinning verification
- [ ] Thread-safety testing
- [ ] Stress testing (navigazione veloce)
- [ ] Power consumption test
- [ ] Touch responsiveness tuning
- [ ] Performance optimization

### Fase 10: Documentazione Codice
**Obiettivo:** Documentare il codice sorgente

- [ ] Doxygen comments per classi pubbliche
- [ ] README.md con getting started
- [ ] Esempi di utilizzo per ogni manager
- [ ] Diagrammi UML (classi, sequenza)
- [ ] Troubleshooting guide

---

## 7. Priorità e Milestone (Aggiornate)

### ~~Milestone 1: "Hello LVGL"~~ ✅ **RAGGIUNTA**
- ✅ Display TFT funzionante
- ✅ LVGL operativo con schermata base
- ✅ Touch funzionante
- ✅ **Demo:** Dashboard interattiva con widget

### ~~Milestone 2: "Core Sistema"~~ ✅ **RAGGIUNTA**
- ✅ EventRouter funzionante
- ✅ ScreenManager con navigazione
- ✅ AppManager con app registry e dock
- ⚠️ SettingsManager con NVS (DA FARE)
- ✅ **Demo:** Navigazione tra 3 schermate (Dashboard, Settings, Info)

### Milestone 3: "Ottimizzazione PSRAM" 🔥 **IN CORSO - PRIORITÀ ALTA**
- [ ] Custom allocator LVGL in PSRAM (512KB)
- [ ] Double buffering PSRAM (30 righe)
- [ ] Wrapper ui_alloc() intelligente
- [ ] Memory monitoring esteso in SystemInfoWidget
- [ ] Testing stress allocazione
- **Demo:** Dashboard con monitoring DRAM/PSRAM/LVGL real-time
- **Target:** +125% DRAM libera, 2x heap LVGL
- **Riferimento:** [DOC/OTTIMIZZAZIONE_PSRAM_LVGL.md](OTTIMIZZAZIONE_PSRAM_LVGL.md)

### Milestone 4: "Servizi Connessi" 🎯 **PROSSIMA**
- [ ] SettingsManager con NVS operativo
- [ ] WiFiService funzionante
- [ ] ServiceManager attivo
- [ ] WiFiStatusWidget creato
- [ ] Settings Screen completa (WiFi config)
- **Demo:** Dashboard con WiFi status live + configurazione WiFi da Settings

### Milestone 5: "Sistema Production-Ready" 📋 **FUTURO**
- [ ] Testing completato (24h stress test)
- [ ] Memory leak verificati assenti
- [ ] Performance profiling
- [ ] Documentazione codice
- [ ] Peripheral Manager (opzionale)
- [ ] Service Layer esteso (BLE, NTP) (opzionale)
- **Demo:** OS stabile pronto per deployment IoT

---

## 8. Dipendenze Critiche

### Hardware
✅ **Disponibili:**
- ESP32-S3 con PSRAM
- Display ILI9341 funzionante
- Touch FT6336U (da testare)

### Software
❌ **Da installare:**
- LVGL 8.4.0
- Libreria touch FT6336U
- ArduinoJson

### Conoscenze Richieste
📚 **Necessarie:**
- LVGL API e sistema di eventi
- FreeRTOS task e semafori
- ESP32 dual-core programming
- I2C/SPI communication
- Memory management PSRAM

---

## 9. Rischi e Mitigazioni

### Rischio 1: Performance LVGL
**Problema:** LVGL potrebbe essere lento senza ottimizzazioni
**Mitigazione:**
- Usare PSRAM per buffer LVGL
- Abilitare DMA su SPI
- Ridurre refresh rate se necessario
- Profiling continuo

### Rischio 2: Touch Controller Non Rilevato
**Problema:** FT6336U potrebbe avere indirizzo I2C diverso
**Mitigazione:**
- I2C scan all'avvio
- Testare librerie alternative
- Verificare pullup resistors

### Rischio 3: Memory Leak
**Problema:** Leak di memoria con LVGL/FreeRTOS
**Mitigazione:**
- RAII pattern per risorse
- Memory profiling frequente
- Cleanup rigoroso in destroy()

### Rischio 4: Thread Deadlock
**Problema:** Deadlock tra Core 0 e Core 1
**Mitigazione:**
- Mutex timeout sempre
- Pattern LVGL lock/unlock verificato
- Testing stress navigazione

---

## 10. Note Tecniche Importanti

### 10.1 USB CDC Serial
Il progetto usa **USB CDC nativo** per la seriale, NON il classico UART.
- Porta seriale: `/dev/ttyACM0` (Linux) o `COM*` (Windows)
- Baudrate: 115200 (ma è ignorato, USB è sempre full speed)
- Attivo all'avvio grazie a `ARDUINO_USB_CDC_ON_BOOT=1`

### 10.2 PSRAM Disponibile ma Non Usato
La board ha PSRAM attivo (flag `-DBOARD_HAS_PSRAM`), ma il codice test TFT NON lo utilizza ancora.

Per usare PSRAM con LVGL:
```cpp
// In lv_conf.h
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_ALLOC ps_malloc
#define LV_MEM_CUSTOM_FREE free
```

### 10.3 Partizioni Flash 16MB
Il file `default_16MB.csv` definisce le partizioni per i 16MB di flash disponibili.

Tipicamente:
- **Factory App:** ~3MB (app principale)
- **OTA Update:** ~3MB (per aggiornamenti OTA)
- **SPIFFS/LittleFS:** ~9MB (file system per risorse UI)

### 10.4 Fix PSRAM Cache Issue
Il flag `-mfix-esp32-psram-cache-issue` risolve un bug hardware noto dell'ESP32-S3 con accesso PSRAM e cache.

**Sempre lasciarlo attivo** per evitare crash randomici.

---

## 11. Comandi Utili PlatformIO

```bash
# Build del progetto
pio run

# Upload su board
pio run --target upload

# Monitor seriale
pio device monitor

# Build + Upload + Monitor (tutto insieme)
pio run --target upload && pio device monitor

# Clean build
pio run --target clean

# Dependency update
pio pkg update

# List installed libraries
pio pkg list
```

---

## 12. Link Documentazione Ufficiale

### ESP32-S3
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [ESP32-S3 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)

### LVGL
- [LVGL Documentation](https://docs.lvgl.io/8.4/)
- [LVGL ESP32 Guide](https://docs.lvgl.io/8.4/get-started/platforms/espressif.html)

### TFT_eSPI
- [TFT_eSPI GitHub](https://github.com/Bodmer/TFT_eSPI)
- [Setup Guide](https://github.com/Bodmer/TFT_eSPI/blob/master/docs/ESP32.md)

### FreeRTOS
- [ESP-IDF FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos.html)

---

## 13. Conclusioni

### Stato Attuale
📍 **Siamo in Fase 0:** Test hardware TFT completato con successo

### Prossimo Passo Immediato
🎯 **Fase 1:** Setup LVGL e integrazione con TFT_eSPI esistente

### Obiettivo Finale
🏁 Un sistema OS-like completo, modulare e production-ready per ESP32-S3 con:
- GUI interattiva e responsive (LVGL)
- Architettura multi-core ottimizzata
- Service layer estensibile
- Widget system riutilizzabile
- Peripheral management centralizzato
- Settings persistenti
- Pronto per applicazioni IoT reali

---

**Documento redatto il:** 2025-11-18
**Autore:** Sistema di documentazione automatico
**Versione:** 1.0.0
