# Stato Attuale del Progetto ESP32-S3 Touch Display

**Data:** 2025-11-18
**Versione Software:** 0.1.0 (Fase Test Hardware)
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

### 3.1 Fase: Test Hardware TFT Display

Il codice attuale (`src/main.cpp`) è un **programma di test** per verificare il funzionamento del display TFT.

**NON è ancora implementato l'OS completo descritto nella documentazione.**

### 3.2 Struttura main.cpp Attuale

```
src/main.cpp (130 righe)
├── Include: Arduino.h, TFT_eSPI.h
├── Namespace anonimo con funzioni helper:
│   ├── enableBacklight() - Accende la retroilluminazione
│   ├── flashSolidColors() - Test colori solidi (rosso, verde, blu, nero, bianco)
│   ├── drawWelcomeBanner() - Schermata di benvenuto con info display
│   └── drawRainbowStripes() - Gradient arcobaleno animato
├── setup()
│   ├── Serial.begin(115200)
│   ├── tft.init()
│   ├── tft.setRotation(1) - Modalità landscape
│   ├── Test colori + Welcome banner
│   └── Prima visualizzazione rainbow
└── loop()
    └── Ridisegna rainbow ogni 6 secondi
```

### 3.3 Funzionalità Attualmente Implementate

✅ **Inizializzazione Display TFT**
- Controller ILI9341 configurato
- Rotazione landscape (320x240)
- Backlight controllato via GPIO 45

✅ **Test Grafici di Base**
- Riempimento schermo colori solidi
- Disegno testo centrato
- Disegno rettangoli arrotondati
- Linee verticali per gradiente
- Font multipli (2, 4, 6)

✅ **Serial Monitor Debug**
- Comunicazione USB CDC @ 115200 baud
- Output log di test

### 3.4 Cosa NON è Ancora Implementato

❌ **Touch Controller FT6336U**
- Nessuna libreria touch inclusa
- Nessun codice di gestione touch

❌ **LVGL (GUI Framework)**
- Libreria non presente nel progetto
- Nessuna UI interattiva

❌ **Architettura OS**
- Nessun Screen Manager
- Nessun App Manager
- Nessun Service Layer
- Nessun Event Router
- Nessun Settings Manager

❌ **FreeRTOS Task Management**
- Nessun task FreeRTOS creato
- Nessun core pinning
- Loop singolo Arduino-style

❌ **Servizi di Sistema**
- Nessun WiFi Service
- Nessun BLE Service
- Nessun NTP Service

❌ **Widget System**
- Nessun widget implementato
- Nessuna dashboard

❌ **Gestione Memoria**
- PSRAM disponibile ma non utilizzato
- Nessun memory manager

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

### Fase 1: Setup Base LVGL (1-2 giorni)
**Obiettivo:** Far funzionare LVGL con il display esistente

- [ ] Installare libreria LVGL 8.4.0
- [ ] Configurare lv_conf.h per ESP32-S3
- [ ] Creare driver LVGL per TFT_eSPI
- [ ] Implementare LVGL tick handler
- [ ] Test: Display "Hello World" con LVGL
- [ ] Configurare PSRAM per buffer LVGL

### Fase 2: Touch Controller (1 giorno)
**Obiettivo:** Integrare il touch FT6336U

- [ ] Installare libreria FT6336U
- [ ] Configurare I2C per touch controller
- [ ] Creare driver LVGL per touch input
- [ ] Test: Touch callback e coordinate
- [ ] Calibrazione touch (se necessaria)

### Fase 3: Core Managers (2-3 giorni)
**Obiettivo:** Implementare i manager fondamentali

#### EventRouter
- [ ] Implementare classe EventRouter
- [ ] Sistema pub/sub thread-safe
- [ ] Test: Pubblicazione e sottoscrizione eventi

#### ScreenManager
- [ ] Implementare classe ScreenManager
- [ ] Stack di navigazione screen
- [ ] Transizioni animate (slide, fade)
- [ ] Test: Navigazione tra 2 screen

#### SettingsManager
- [ ] Implementare classe SettingsManager
- [ ] Persistenza NVS (Preferences)
- [ ] Type-safe getters/setters
- [ ] Callback onChange
- [ ] Test: Salvataggio e caricamento settings

#### AppManager
- [ ] Implementare classe AppManager
- [ ] Registry applicazioni
- [ ] App lifecycle (create, start, stop, destroy)
- [ ] Factory pattern per app
- [ ] Test: Registrazione e lancio app dummy

### Fase 4: Service Layer (3-4 giorni)
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

### Fase 5: Widget System (2-3 giorni)
**Obiettivo:** Creare widget riutilizzabili per UI

#### Widget Base Class
- [ ] Classe base Widget astratta
- [ ] Thread-safe update methods
- [ ] Event subscription
- [ ] Layout support (flex/grid)
- [ ] Test: Widget dummy

#### WiFiStatusWidget
- [ ] RSSI signal bar
- [ ] IP address display
- [ ] State colors (connecting/connected/error)
- [ ] Test: Integrazione con WiFiService

#### SystemInfoWidget
- [ ] CPU usage indicator
- [ ] Memory usage bar (RAM/PSRAM)
- [ ] Uptime counter
- [ ] Test: Aggiornamento real-time

#### ClockWidget (opzionale)
- [ ] Time/date display
- [ ] NTP sync indicator
- [ ] Format customization
- [ ] Test: Integrazione con NTPService

### Fase 6: Peripheral Manager Layer (3-4 giorni)
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

### Fase 7: Dashboard Application (2 giorni)
**Obiettivo:** Creare la dashboard principale

- [ ] Implementare DashboardScreen
- [ ] Layout grid per widgets
- [ ] Integrare WiFiStatusWidget
- [ ] Integrare SystemInfoWidget
- [ ] Integrare ClockWidget (se fatto)
- [ ] Test: Dashboard completa

### Fase 8: Settings Application (2 giorni)
**Obiettivo:** Schermata impostazioni

- [ ] Implementare SettingsScreen
- [ ] WiFi SSID/Password input
- [ ] Display brightness slider
- [ ] Theme selector (light/dark)
- [ ] About section
- [ ] Test: Modifica e salvataggio settings

### Fase 9: Testing & Ottimizzazione (2-3 giorni)
**Obiettivo:** Stabilità e performance

- [ ] Memory profiling (leak detection)
- [ ] Core pinning verification
- [ ] Thread-safety testing
- [ ] Stress testing (navigazione veloce)
- [ ] Power consumption test
- [ ] Touch responsiveness tuning
- [ ] Performance optimization

### Fase 10: Documentazione Codice (1-2 giorni)
**Obiettivo:** Documentare il codice sorgente

- [ ] Doxygen comments per classi pubbliche
- [ ] README.md con getting started
- [ ] Esempi di utilizzo per ogni manager
- [ ] Diagrammi UML (classi, sequenza)
- [ ] Troubleshooting guide

---

## 7. Stima Tempi Totale

| Fase | Giorni | Giorni Cumulativi |
|------|--------|-------------------|
| **Fase 1:** Setup LVGL | 1-2 | 2 |
| **Fase 2:** Touch Controller | 1 | 3 |
| **Fase 3:** Core Managers | 2-3 | 6 |
| **Fase 4:** Service Layer | 3-4 | 10 |
| **Fase 5:** Widget System | 2-3 | 13 |
| **Fase 6:** Peripheral Manager | 3-4 | 17 |
| **Fase 7:** Dashboard App | 2 | 19 |
| **Fase 8:** Settings App | 2 | 21 |
| **Fase 9:** Testing | 2-3 | 24 |
| **Fase 10:** Docs | 1-2 | 26 |

**Totale stimato:** 20-26 giorni lavorativi (4-5 settimane a tempo pieno)

---

## 8. Priorità e Milestone

### Milestone 1: "Hello LVGL" (Fine settimana 1)
- ✅ Display TFT funzionante (FATTO)
- 🎯 LVGL operativo con schermata base
- 🎯 Touch funzionante
- **Demo:** Pulsante cliccabile su schermo

### Milestone 2: "Core Sistema" (Fine settimana 2)
- 🎯 EventRouter funzionante
- 🎯 ScreenManager con navigazione
- 🎯 SettingsManager con NVS
- 🎯 AppManager con app registry
- **Demo:** Navigazione tra 3 schermate

### Milestone 3: "Servizi Attivi" (Fine settimana 3)
- 🎯 WiFiService connesso
- 🎯 ServiceManager attivo
- 🎯 Widget base creati
- **Demo:** Dashboard con WiFi status live

### Milestone 4: "Sistema Completo" (Fine settimana 4-5)
- 🎯 Dashboard completa
- 🎯 Settings funzionanti
- 🎯 Peripheral Manager operativo
- 🎯 Testing completato
- **Demo:** OS completo pronto per deployment

---

## 9. Dipendenze Critiche

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

## 10. Rischi e Mitigazioni

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

## 11. Note Tecniche Importanti

### 11.1 USB CDC Serial
Il progetto usa **USB CDC nativo** per la seriale, NON il classico UART.
- Porta seriale: `/dev/ttyACM0` (Linux) o `COM*` (Windows)
- Baudrate: 115200 (ma è ignorato, USB è sempre full speed)
- Attivo all'avvio grazie a `ARDUINO_USB_CDC_ON_BOOT=1`

### 11.2 PSRAM Disponibile ma Non Usato
La board ha PSRAM attivo (flag `-DBOARD_HAS_PSRAM`), ma il codice test TFT NON lo utilizza ancora.

Per usare PSRAM con LVGL:
```cpp
// In lv_conf.h
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_ALLOC ps_malloc
#define LV_MEM_CUSTOM_FREE free
```

### 11.3 Partizioni Flash 16MB
Il file `default_16MB.csv` definisce le partizioni per i 16MB di flash disponibili.

Tipicamente:
- **Factory App:** ~3MB (app principale)
- **OTA Update:** ~3MB (per aggiornamenti OTA)
- **SPIFFS/LittleFS:** ~9MB (file system per risorse UI)

### 11.4 Fix PSRAM Cache Issue
Il flag `-mfix-esp32-psram-cache-issue` risolve un bug hardware noto dell'ESP32-S3 con accesso PSRAM e cache.

**Sempre lasciarlo attivo** per evitare crash randomici.

---

## 12. Comandi Utili PlatformIO

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

## 13. Link Documentazione Ufficiale

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

## 14. Conclusioni

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
