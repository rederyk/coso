# Documentazione Architettura OS ESP32 S3 Display Touch

## 📚 Guida alla Documentazione

Questo progetto contiene la documentazione completa per implementare un sistema operativo leggero (OS-like) su ESP32 S3 con display touch.

---

## 🚀 Documenti Principali (LEGGI QUESTI!)

### 1. [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) ⭐ **DOCUMENTO PRINCIPALE**

**Documento unificato e completo dell'architettura.**

Contiene:
- ✅ Architettura completa a 5 layer (Hardware → HAL → GUI → Managers → Services → Apps)
- ✅ Stack tecnologico (LVGL, TFT_eSPI, FT6336U)
- ✅ Core Managers (Screen, App, Settings, EventRouter)
- ✅ Service Layer (WiFi, BLE con esempi completi)
- ✅ Widget System (WiFi Status, System Info, Clock)
- ✅ Thread-safety patterns (mutex LVGL, core pinning)
- ✅ FreeRTOS task map e priorità
- ✅ Inizializzazione sistema completa
- ✅ Best practices e problemi comuni risolti
- ✅ Configurazione PlatformIO
- ✅ Esempi di codice ready-to-use

**👉 Inizia da qui per capire l'architettura completa!**

---

### 2. [ESTENSIONE_PERIPHERAL_MANAGER.md](ESTENSIONE_PERIPHERAL_MANAGER.md) ⭐ **GESTIONE HARDWARE**

**Estensione per gestione periferiche hardware (GPIO, I2C, SPI, ADC, PWM, UART).**

Contiene:
- ✅ Peripheral Manager Layer
- ✅ GPIO Manager (allocazione, interrupt, ownership tracking)
- ✅ I2C Manager (multi-bus, device scan, hot-plug)
- ✅ Pattern per SPI/ADC/PWM/UART Manager
- ✅ Resource management e prevenzione conflitti
- ✅ API thread-safe per tutte le periferiche
- ✅ Esempi di app e servizi che usano periferiche

**👉 Leggi questo per aggiungere supporto hardware (sensori, LED, pulsanti, ecc.)!**

---

## 📁 Documenti di Riferimento Storico

> **⚠️ ATTENZIONE**: Questi documenti sono **deprecati** e mantenuti solo per riferimento storico.
>
> **Non usarli per l'implementazione!** Usa invece i documenti principali sopra.

### [REPORT_ARCHITETTURA_OS_ESP32.md](REPORT_ARCHITETTURA_OS_ESP32.md) - ⛔ DEPRECATO

- Prima versione dell'architettura base
- **Sostituito da**: [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md)

### [ESTENSIONI_ARCHITETTURA_SERVIZI.md](ESTENSIONI_ARCHITETTURA_SERVIZI.md) - ⛔ DEPRECATO

- Estensioni Service Layer e Widget System
- **Integrato in**: [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md)

---

## 🗺️ Mappa della Documentazione

```
Documentazione OS ESP32 S3
│
├── 📘 REPORT_COMPLETO_OS_ESP32.md ← INIZIA QUI!
│   ├── Architettura completa
│   ├── Core Managers
│   ├── Service Layer
│   ├── Widget System
│   ├── Thread-safety
│   └── Esempi completi
│
├── 📗 ESTENSIONE_PERIPHERAL_MANAGER.md ← PER HARDWARE
│   ├── GPIO Manager
│   ├── I2C Manager
│   ├── Pattern per altre periferiche
│   └── Esempi periferiche
│
└── 📂 Riferimenti Storici (NON USARE)
    ├── REPORT_ARCHITETTURA_OS_ESP32.md (deprecato)
    └── ESTENSIONI_ARCHITETTURA_SERVIZI.md (deprecato)
```

---

## 🎯 Percorso di Lettura Consigliato

### Per chi inizia da zero:

1. **[REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md)** (Sezioni 1-7)
   - Capire stack tecnologico
   - Architettura a 5 layer
   - Regole core pinning e thread-safety
   - Core Managers (Screen, App, Settings, EventRouter)

2. **[REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md)** (Sezioni 4-5)
   - Service Layer (WiFi, BLE)
   - Widget System (dashboard customizzabile)

3. **[ESTENSIONE_PERIPHERAL_MANAGER.md](ESTENSIONE_PERIPHERAL_MANAGER.md)**
   - GPIO Manager per LED/pulsanti
   - I2C Manager per sensori

4. **[REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md)** (Sezioni 7-8)
   - Inizializzazione sistema completa
   - Configurazione PlatformIO

### Per chi vuole aggiungere hardware:

1. **[ESTENSIONE_PERIPHERAL_MANAGER.md](ESTENSIONE_PERIPHERAL_MANAGER.md)** (Sezioni 2-4)
   - Peripheral Base Class
   - GPIO Manager
   - I2C Manager

2. **[ESTENSIONE_PERIPHERAL_MANAGER.md](ESTENSIONE_PERIPHERAL_MANAGER.md)** (Sezione 6)
   - Esempi di utilizzo
   - Integrazione con architettura esistente

### Per chi vuole aggiungere servizi/app:

1. **[REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md)** (Sezione 4)
   - Service Base Class
   - Esempi WiFi/BLE Service

2. **[REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md)** (Sezione 3)
   - Application Manager
   - Screen Manager

---

## 📊 Architettura Completa

```
┌─────────────────────────────────────────────────────────────────┐
│                     APPLICATION LAYER                            │
│  (Apps, Dashboard, Settings)                                     │
└─────────────────────────────────────────────────────────────────┘
                             │
┌─────────────────────────────────────────────────────────────────┐
│              SYSTEM SERVICES LAYER                               │
│  (WiFi, BLE, NTP, MQTT)                                          │
└─────────────────────────────────────────────────────────────────┘
                             │
┌─────────────────────────────────────────────────────────────────┐
│              PERIPHERAL MANAGER LAYER                            │
│  (GPIO, I2C, SPI, ADC, PWM, UART)                                │
└─────────────────────────────────────────────────────────────────┘
                             │
┌─────────────────────────────────────────────────────────────────┐
│              CORE MANAGERS LAYER                                 │
│  (Screen, App, Settings, EventRouter)                            │
└─────────────────────────────────────────────────────────────────┘
                             │
┌─────────────────────────────────────────────────────────────────┐
│              GUI FRAMEWORK LAYER (LVGL)                          │
└─────────────────────────────────────────────────────────────────┘
                             │
┌─────────────────────────────────────────────────────────────────┐
│              HARDWARE ABSTRACTION LAYER                          │
│  (TFT_eSPI, FT6336U, ESP32 HAL)                                  │
└─────────────────────────────────────────────────────────────────┘
                             │
┌─────────────────────────────────────────────────────────────────┐
│                        HARDWARE                                  │
│  ESP32-S3 Dual Core + PSRAM + Display + Touch                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## ✅ Checklist Implementazione

### Fase 1: Setup Base
- [ ] Leggere [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) sezioni 1-2
- [ ] Configurare PlatformIO (sezione 8)
- [ ] Inizializzare LVGL con mutex (sezione 7)
- [ ] Verificare core pinning (WiFi Core 0, LVGL Core 1)

### Fase 2: Core Managers
- [ ] Implementare EventRouter (sezione 3.4)
- [ ] Implementare ScreenManager (sezione 3.1)
- [ ] Implementare SettingsManager (sezione 3.3)
- [ ] Implementare AppManager (sezione 3.2)

### Fase 3: Service Layer
- [ ] Implementare Service Base Class (sezione 4.1)
- [ ] Implementare ServiceManager (sezione 4.4)
- [ ] Implementare WiFiService (sezione 4.2)
- [ ] Implementare BLEService (sezione 4.3)

### Fase 4: Peripheral Manager (opzionale)
- [ ] Leggere [ESTENSIONE_PERIPHERAL_MANAGER.md](ESTENSIONE_PERIPHERAL_MANAGER.md)
- [ ] Implementare PeripheralManager (sezione 5)
- [ ] Implementare GPIOManager (sezione 3)
- [ ] Implementare I2CManager (sezione 4)

### Fase 5: Widget System
- [ ] Implementare DashboardWidget base (sezione 5.1)
- [ ] Implementare WiFiStatusWidget (sezione 5.2)
- [ ] Implementare SystemInfoWidget (sezione 5.3)
- [ ] Implementare ClockWidget (sezione 5.4)

### Fase 6: Dashboard
- [ ] Creare CustomDashboard screen
- [ ] Aggiungere widget alla dashboard
- [ ] Configurare layout grid

### Fase 7: Testing & Debug
- [ ] Test thread-safety (WiFi + LVGL simultanei)
- [ ] Test memory usage con printMemoryStats()
- [ ] Test stress (WiFi + BLE + UI)
- [ ] Verificare checklist pre-deployment (sezione 13)

---

## 🔧 Configurazione Rapida

### platformio.ini

```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

build_flags =
    -D LV_CONF_INCLUDE_SIMPLE
    -D BOARD_HAS_PSRAM
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -D LV_HOR_RES_MAX=320
    -D LV_VER_RES_MAX=240

lib_deps =
    lvgl/lvgl@^8.4.0
    bodmer/TFT_eSPI@^2.5.43

monitor_speed = 115200
board_build.flash_mode = qio
board_build.partitions = huge_app.csv
```

---

## 📖 Risorse Aggiuntive

### Documentazione Ufficiale
- **LVGL**: https://docs.lvgl.io/
- **LVGL + FreeRTOS**: https://docs.lvgl.io/master/integration/os/freertos.html
- **ESP32 Programming Guide**: https://docs.espressif.com/
- **TFT_eSPI**: https://github.com/Bodmer/TFT_eSPI

### Community
- **ESP32 Forum**: https://esp32.com/
- **LVGL Forum**: https://forum.lvgl.io/

### Progetti di Riferimento
- **Tactility OS**: https://github.com/ByteWelder/Tactility
- **LVGL ESP32 Port**: https://github.com/lvgl/lv_port_esp32

---

## 🆘 Supporto

### Problemi Comuni

**UI freeze?**
→ Vedi [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) sezione 11.1

**WiFi/BLE lento?**
→ Vedi [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) sezione 11.2

**Memory leak?**
→ Vedi [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) sezione 11.3

**Conflitti GPIO/I2C?**
→ Vedi [ESTENSIONE_PERIPHERAL_MANAGER.md](ESTENSIONE_PERIPHERAL_MANAGER.md) sezioni 3-4

---

## 📝 Licenze

Questo progetto utilizza librerie con le seguenti licenze:
- **LVGL**: MIT License
- **TFT_eSPI**: FreeBSD License
- **ESP32 Arduino**: LGPL 2.1

Vedi file originali per dettagli completi delle licenze.

---

## 🚀 Quick Start

```bash
# 1. Clone progetto
git clone <your-repo>
cd esp32_s3_display

# 2. Leggi documentazione principale
cat REPORT_COMPLETO_OS_ESP32.md

# 3. Configura PlatformIO
pio init

# 4. Implementa core managers seguendo sezioni 3-4

# 5. Build e upload
pio run --target upload

# 6. Monitor seriale
pio device monitor
```

---

**Versione Documentazione:** 2.0
**Data:** 2025-11-14
**Stato:** ✅ Production Ready

---

## 📌 Note Importanti

1. **Thread-Safety è CRITICO**: Leggi sezione 6 di REPORT_COMPLETO_OS_ESP32.md
2. **Core Pinning OBBLIGATORIO**: WiFi/BLE Core 0, LVGL Core 1
3. **PSRAM Raccomandato**: Per buffer LVGL grandi
4. **Mutex Globale**: SEMPRE prima di chiamare funzioni LVGL da altri task

**Buona implementazione! 🎉**
