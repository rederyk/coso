# Indice Generale Documentazione - OS ESP32 S3

## 📑 Indice Rapido

| Documento | Tipo | Status | Descrizione |
|-----------|------|--------|-------------|
| **[README_DOCUMENTAZIONE.md](README_DOCUMENTAZIONE.md)** | Guida | ⭐ INIZIA QUI | Guida navigazione e quick start |
| **[STATO_ATTUALE_PROGETTO.md](STATO_ATTUALE_PROGETTO.md)** | Status | 🎯 CORRENTE | Stato reale progetto e roadmap |
| **[REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md)** | Principale | 📋 PIANIFICATO | Architettura completa OS (target) |
| **[OTTIMIZZAZIONE_PSRAM_LVGL.md](OTTIMIZZAZIONE_PSRAM_LVGL.md)** | Tecnico | ⚡ NUOVO | Strategia PSRAM 8MB per LVGL e grafica |
| **[ARCHITETTURA_UI_THEME_SYSTEM.md](ARCHITETTURA_UI_THEME_SYSTEM.md)** | UI/UX | 📋 PIANIFICATO | Sistema temi JSON e asset manager |
| **[ESTENSIONE_PERIPHERAL_MANAGER.md](ESTENSIONE_PERIPHERAL_MANAGER.md)** | Estensione | 📋 PIANIFICATO | Gestione periferiche hardware |
| **[CHANGELOG_DOCUMENTAZIONE.md](CHANGELOG_DOCUMENTAZIONE.md)** | Changelog | ℹ️ Info | Cronologia modifiche |
| [REPORT_ARCHITETTURA_OS_ESP32.md](REPORT_ARCHITETTURA_OS_ESP32.md) | Riferimento | ⛔ DEPRECATO | Architettura base (v1.0) |
| [ESTENSIONI_ARCHITETTURA_SERVIZI.md](ESTENSIONI_ARCHITETTURA_SERVIZI.md) | Riferimento | ⛔ DEPRECATO | Estensioni servizi (v1.0) |

---

## ⚡ OTTIMIZZAZIONE_PSRAM_LVGL.md

### Informazioni Contenute
- **Strategia PSRAM:** Piano completo per ottimizzare 8MB PSRAM ESP32-S3
- **Custom Allocator LVGL:** Spostare heap LVGL da DRAM a PSRAM
- **Double Buffering:** 30 righe in PSRAM per rendering fluido
- **Memory Monitoring:** Dashboard real-time DRAM/PSRAM/LVGL
- **Wrapper ui_alloc():** Allocazione intelligente con fallback
- **Best Practices:** Latenza, cache coherency, thread safety

### Sezioni Principali
1. Executive Summary (obiettivi e benefici attesi)
2. **Analisi Stato Attuale** (configurazione corrente + problemi)
3. **Strategia Ottimizzazione** (architettura target)
4. **Piano Implementazione** (6 fasi dettagliate)
   - FASE 1: Custom Allocator LVGL in PSRAM
   - FASE 2: Double Buffering in PSRAM
   - FASE 3: Wrapper ui_alloc() per Widget
   - FASE 4: Memory Monitoring Dashboard
   - FASE 5: Sprite Pool (opzionale futuro)
   - FASE 6: Testing e Validazione
5. **Checklist Implementazione** (priorità alta/media/bassa)
6. **Benefici e Trade-off** (tabelle comparative)
7. **Note Tecniche** (latenza, cache coherency, overhead)
8. **Troubleshooting** (4 problemi comuni + soluzioni)
9. **Esempi Completi** (codice ready-to-use)
10. **Conclusioni** (risultati attesi + manutenzione)
11. **Roadmap** (3 settimane implementazione)
12. **Appendici** (riferimenti + codice sprite pool)

### Caratteristiche Chiave

**Custom Allocator LVGL:**
- `LV_MEM_CUSTOM=1` con funzioni `lvgl_malloc/free/realloc`
- Priorità PSRAM con fallback automatico DRAM
- Logging dettagliato allocazioni per debug
- Monitoring frammentazione heap interno LVGL

**Memory Layout Ottimizzato:**
```
DRAM (~520KB)           PSRAM (8MB)
├─ FreeRTOS (~250KB)    ├─ LVGL Heap (512KB) ← NUOVO
├─ Stack Tasks (~100KB) ├─ Draw Buffer 1 (19KB) ← NUOVO
├─ Static/BSS (~100KB)  ├─ Draw Buffer 2 (19KB) ← NUOVO
└─ LIBERA (~450KB) ✅   ├─ Widget Pool (dinamico)
                         └─ LIBERA (~7MB) ✅
```

**Wrapper Intelligente:**
- `ui_alloc(size, label)` - Prova PSRAM → fallback DRAM
- `sprite_alloc(size)` - Allocazione aligned per TFT_eSPI sprite
- Logging automatico con etichette debug

**Dashboard Monitoring:**
- Label real-time: DRAM free, PSRAM free/total, LVGL heap usage
- Warning visivo se frammentazione >20%
- Colori dinamici basati su threshold utilizzo
- Refresh ogni 1 secondo

**Benefici Attesi:**
| Metrica | Prima | Dopo | Miglioramento |
|---------|-------|------|---------------|
| DRAM libera | ~200KB | ~450KB | +125% |
| LVGL heap | 256KB DRAM | 512KB PSRAM | 2x capacità |
| Draw buffer | 12.5KB DRAM | 37.5KB PSRAM | 3x dimensione |

**File da Creare:**
- `src/utils/psram_allocator.h` (header API)
- `src/utils/psram_allocator.cpp` (implementazione)
- `src/utils/sprite_pool.h` (opzionale futuro)

**File da Modificare:**
- `src/config/lv_conf.h` → `LV_MEM_CUSTOM=1`
- `src/main.cpp` → double buffering setup
- `src/widgets/system_info_widget.h` → nuove label
- `src/widgets/system_info_widget.cpp` → monitoring PSRAM/LVGL

**📋 Status:** PRONTO PER IMPLEMENTAZIONE - Piano dettagliato con codice completo

---

## 🎯 STATO_ATTUALE_PROGETTO.md

### Informazioni Contenute
- **Hardware Reale:** Specifiche Freenove ESP32-S3 Display 2.8"
- **Pinout TFT:** Mappatura completa GPIO (ILI9341)
- **Configurazione PlatformIO:** Build flags e impostazioni
- **Stato Software:** Fase test TFT (pre-OS)
- **Roadmap Implementazione:** 10 fasi dettagliate
- **Stima Tempi:** 20-26 giorni lavorativi
- **Milestone:** 4 milestone principali
- **Dipendenze:** Hardware, software, conoscenze
- **Rischi:** Identificazione e mitigazioni
- **Comandi Utili:** PlatformIO quick reference

### Sezioni Principali
1. Specifiche Hardware Reali
2. Configurazione PlatformIO
3. Stato Attuale Software (Test TFT)
4. Librerie Presenti
5. Struttura File System
6. **Roadmap Implementazione (10 fasi)**
7. Stima Tempi Totale
8. Priorità e Milestone
9. Dipendenze Critiche
10. Rischi e Mitigazioni
11. Note Tecniche
12. Comandi Utili
13. Link Documentazione
14. Conclusioni

**📍 IMPORTANTE:** Questo è il documento da consultare per capire lo stato REALE del progetto e i prossimi passi concreti.

---

## 🎨 ARCHITETTURA_UI_THEME_SYSTEM.md

### Informazioni Contenute
- **Theme System JSON-based** - Temi configurabili via file JSON
- **Asset Manager** - Gestione centralizzata icone, font, immagini
- **Style Manager** - Applicazione dinamica stili LVGL
- **Hot-reload** - Cambio tema runtime senza riavvio
- **Esempi Temi** - Light, Dark, High Contrast completi

### Sezioni Principali
1. Executive Summary
2. Architettura Generale (Stack UI completo)
3. **Theme Manager** (Classe + API)
4. **Struttura JSON del Tema** (Schema completo + esempi)
5. **Asset Manager** (Gestione risorse grafiche)
6. **Style Manager** (Applicazione stili LVGL)
7. Esempi Temi Predefiniti (Light/Dark/High Contrast)
8. **Pacchetti Grafici** (Asset Packs JSON)
9. Integrazione con LVGL
10. Settings UI per Cambio Tema
11. File System Layout (SPIFFS/LittleFS)
12. Ottimizzazioni e Best Practices
13. **Roadmap Implementazione** (7 fasi, 9-12 giorni)
14. Esempi Codice Completi
15. Conclusioni

### Caratteristiche Chiave

**Theme System:**
- JSON completo con metadata, colors, typography, dimensions, components, animations
- Sistema di riferimenti `$variabili` per riuso valori
- Supporto stati (hover, pressed, disabled)
- Hot-reload runtime

**Asset Manager:**
- Caricamento lazy di icone/font/immagini
- LRU cache automatica
- Pre-loading asset pack
- Supporto PSRAM per asset grandi

**Style Manager:**
- Builder component styles (button, card, input, navbar, ecc.)
- Macro helper per applicazione rapida
- Transizioni animate tra stili

**File System:**
- Layout SPIFFS organizzato (/themes, /icons, /fonts, /images)
- Partizionamento flash custom (~10MB per SPIFFS)
- Asset pack modulari

**📋 Status:** PIANIFICATO - Da implementare in Fase 5-6 della roadmap generale (dopo Widget System)

---

## 📘 REPORT_COMPLETO_OS_ESP32.md

### Sezione 1: Stack Tecnologico
- 1.1 Librerie Fondamentali
  - LVGL v8.4.0
  - TFT_eSPI v2.5.43
  - FT6336U Touch Controller
- 1.2 Architettura Hardware
  - ESP32-S3 Dual Core
  - Display TFT
  - PSRAM

### Sezione 2: Architettura Software
- 2.1 Struttura Modulare a 5 Livelli
  - Application Layer
  - Services Layer
  - Peripheral Manager Layer ⭐ NUOVO
  - Core Managers Layer
  - GUI Framework Layer
  - HAL Layer
  - Hardware Layer
- 2.2 Regole Architetturali CRITICHE
  - Core Pinning (WiFi Core 0, LVGL Core 1)
  - Tabella Priorità FreeRTOS

### Sezione 3: Componenti Core del Sistema
- 3.1 Screen Manager
  - Navigazione schermate
  - Stack management
  - Transizioni animate
- 3.2 Application Manager
  - Registrazione app
  - Lifecycle management
  - App factory pattern
- 3.3 Settings Manager
  - Persistenza NVS
  - Callback onChange
  - Type-safe getters/setters
- 3.4 Event Router
  - Pub/Sub pattern
  - Thread-safe communication
  - Event subscription

### Sezione 4: Service Layer
- 4.1 Service Base Class
  - Lifecycle hooks
  - Core pinning automatico
  - Task wrapper
- 4.2 WiFi Service
  - Auto-reconnect
  - State machine
  - Event publishing
- 4.3 BLE Service
  - Advertising management
  - Connection callbacks
  - Event-driven
- 4.4 Service Manager
  - Registry centralizzato
  - Start/stop all
  - Status monitoring

### Sezione 5: Widget System
- 5.1 Widget Base Class
  - Thread-safe updates
  - Event subscription
  - Layout support
- 5.2 WiFi Status Widget
  - RSSI bar
  - IP display
  - State colors
- 5.3 System Info Widget
  - CPU info
  - Memory usage bar
  - Uptime counter
- 5.4 Clock Widget
  - NTP sync
  - Time/date display
  - Format customization

### Sezione 6: Thread-Safety
- 6.1 Pattern LVGL Mutex
  - Global mutex setup
  - Task locking
  - Timeout handling
- 6.2 Regole d'Oro
  - ✅ DA FARE SEMPRE
  - ❌ DA NON FARE MAI
  - Esempi pratici

### Sezione 7: Inizializzazione Sistema
- 7.1 Setup Completo
  - Ordine critico operations
  - LVGL init
  - Services init
  - Task creation

### Sezione 8: Configurazione PlatformIO
- platformio.ini
- Build flags
- Lib deps
- Board config

### Sezione 9: Struttura File System
- src/ organization
- core/ components
- services/ layer
- widgets/ dashboard
- apps/ user apps
- data/ SPIFFS

### Sezione 10: FreeRTOS Task Map
- Core 0 tasks
- Core 1 tasks
- Priority table
- Timer interrupts

### Sezione 11: Problemi Comuni
- 11.1 UI Freeze
  - Cause
  - Debug
  - Soluzioni
- 11.2 WiFi/BLE Lento
  - Priority tuning
  - Yield insertion
- 11.3 Memory Leak
  - Monitoring
  - Cleanup patterns

### Sezione 12: Best Practices
- 12.1 Memory Management
  - PSRAM usage
  - Cleanup onDestroy
- 12.2 MVC Pattern
  - Model/View/Controller
  - Separation of concerns
- 12.3 Error Handling
  - Result pattern
  - Error propagation

### Sezione 13: Checklist Pre-Deployment
- Core pinning verificato
- Thread-safety testata
- Memory profiling
- Stress testing

### Sezione 14: Vantaggi Architettura
- Modularità
- Thread-safety
- Multi-core optimization
- Extensibility

### Sezione 15: Limitazioni
- RAM constraints
- Multi-thread complexity
- Core pinning requirements

### Sezione 16: Prossimi Passi
- Base implementation
- Future extensions
- Roadmap

### Sezione 17: Risorse
- Documentazione ufficiale
- Community resources
- Progetti di riferimento

---

## 📗 ESTENSIONE_PERIPHERAL_MANAGER.md

### Sezione 1: Architettura Estesa
- 1.1 Nuovo Layer nella Stack
  - Peripheral Manager Layer
  - Posizionamento architetturale

### Sezione 2: Peripheral Manager Core
- 2.1 Peripheral Base Class
  - PeripheralType enum
  - PeripheralState enum
  - Allocation/deallocation
  - Event publishing

### Sezione 3: GPIO Manager
- 3.1 GPIO Peripheral
  - Mode configuration
  - Read/write operations
  - Toggle support
  - Interrupt handling
- 3.2 GPIO Manager Class
  - Request/release GPIO
  - Ownership tracking
  - Conflict detection
  - Status monitoring

### Sezione 4: I2C Manager
- 4.1 I2C Bus Management
  - Multi-bus support
  - Device scanning
  - Hot-plug detection
- 4.2 I2C Peripheral Class
  - Bus initialization
  - Device allocation
  - Thread-safe operations
- 4.3 I2C Operations
  - Read/write
  - Register access
  - Transaction queueing

### Sezione 5: Peripheral Manager Registry
- 5.1 PeripheralManager Class
  - Centralized registry
  - Manager getters
  - Status printing

### Sezione 6: Esempi di Utilizzo
- 6.1 Inizializzazione Sistema
  - I2C bus init
  - GPIO allocation
  - Status check
- 6.2 App con GPIO
  - LED controller example
  - Resource cleanup
- 6.3 Service con I2C
  - Sensor service
  - Device allocation
  - Temperature reading

### Sezione 7: Vantaggi
- Resource management
- Conflict prevention
- Thread-safety
- Hot-plug ready

### Sezione 8: Manager Aggiuntivi
- 8.1 SPI Manager (TODO)
- 8.2 ADC Manager (TODO)
- 8.3 PWM Manager (TODO)
- 8.4 UART Manager (TODO)

### Sezione 9: Integrazione
- 9.1 Setup completo
  - Peripheral init
  - Service integration
- 9.2 Dashboard widgets
  - GPIO monitor
  - Sensor widget

### Sezione 10: Conclusioni
- Modularità verificata
- Vantaggi chiave
- Prossimi passi

---

## 📖 README_DOCUMENTAZIONE.md

### Sezioni Principali
1. Documenti Principali
   - REPORT_COMPLETO_OS_ESP32.md
   - ESTENSIONE_PERIPHERAL_MANAGER.md
2. Documenti Deprecati
   - REPORT_ARCHITETTURA_OS_ESP32.md
   - ESTENSIONI_ARCHITETTURA_SERVIZI.md
3. Mappa Documentazione
4. Percorsi di Lettura
   - Per chi inizia da zero
   - Per chi vuole aggiungere hardware
   - Per chi vuole aggiungere servizi/app
5. Architettura Completa (diagramma)
6. Checklist Implementazione
   - Fase 1: Setup Base
   - Fase 2: Core Managers
   - Fase 3: Service Layer
   - Fase 4: Peripheral Manager
   - Fase 5: Widget System
   - Fase 6: Dashboard
   - Fase 7: Testing & Debug
7. Configurazione Rapida
8. Risorse Aggiuntive
9. Supporto (problemi comuni)
10. Quick Start

---

## 📝 CHANGELOG_DOCUMENTAZIONE.md

### Versione 2.0.0 - 2025-11-14
- ✨ Nuovi Documenti
  - REPORT_COMPLETO_OS_ESP32.md
  - ESTENSIONE_PERIPHERAL_MANAGER.md
  - README_DOCUMENTAZIONE.md
- 🔄 Modifiche
  - REPORT_ARCHITETTURA_OS_ESP32.md (deprecato)
  - ESTENSIONI_ARCHITETTURA_SERVIZI.md (deprecato)
- ✨ Nuove Funzionalità
  - Peripheral Manager Layer
  - GPIO Manager
  - I2C Manager
- 📊 Confronto Versioni
  - v1.0 vs v2.0
  - Statistiche
- 🔮 Roadmap
  - v2.1, v2.2, v3.0
- 📝 Documentazione Migliorata
  - +7 sezioni nuove
  - +17 esempi codice
  - +7 problemi comuni risolti

---

## 🗺️ Mappa Concettuale

```
┌─────────────────────────────────────────────────────────────────┐
│                  DOCUMENTAZIONE OS ESP32 S3                      │
└─────────────────────────────────────────────────────────────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
        ▼                    ▼                    ▼
┌───────────────┐  ┌──────────────────┐  ┌──────────────────┐
│ README_DOC    │  │ REPORT_COMPLETO  │  │ ESTENSIONE_      │
│ (Guida)       │  │ (Architettura)   │  │ PERIPHERAL_MGR   │
│               │  │                  │  │ (Hardware)       │
│ - Quick Start │  │ - 5 Layer Arch   │  │ - GPIO Manager   │
│ - Navigation  │  │ - Core Managers  │  │ - I2C Manager    │
│ - Checklist   │  │ - Services       │  │ - Patterns       │
└───────────────┘  │ - Widgets        │  └──────────────────┘
                   │ - Thread-Safety  │
                   │ - FreeRTOS       │
                   └──────────────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
        ▼                    ▼                    ▼
┌───────────────┐  ┌──────────────────┐  ┌──────────────────┐
│ CHANGELOG     │  │ REPORT_OLD       │  │ ESTENSIONI_OLD   │
│ (History)     │  │ (Deprecato)      │  │ (Deprecato)      │
└───────────────┘  └──────────────────┘  └──────────────────┘
```

---

## 📊 Metriche Documentazione

### Copertura Argomenti

| Categoria | Status | Documenti |
|-----------|--------|-----------|
| **Architettura Base** | ✅ 100% | REPORT_COMPLETO |
| **Service Layer** | ✅ 100% | REPORT_COMPLETO |
| **Widget System** | ✅ 100% | REPORT_COMPLETO |
| **Thread-Safety** | ✅ 100% | REPORT_COMPLETO |
| **GPIO Manager** | ✅ 100% | ESTENSIONE_PERIPHERAL |
| **I2C Manager** | ✅ 100% | ESTENSIONE_PERIPHERAL |
| **SPI Manager** | 🔄 Pattern | ESTENSIONE_PERIPHERAL |
| **ADC Manager** | 🔄 Pattern | ESTENSIONE_PERIPHERAL |
| **PWM Manager** | 🔄 Pattern | ESTENSIONE_PERIPHERAL |
| **UART Manager** | 🔄 Pattern | ESTENSIONE_PERIPHERAL |

### Esempi di Codice

| Componente | Esempi | Status |
|------------|--------|--------|
| Screen Manager | 2 | ✅ Completi |
| App Manager | 2 | ✅ Completi |
| Settings Manager | 2 | ✅ Completi |
| EventRouter | 3 | ✅ Completi |
| WiFi Service | 1 | ✅ Completo |
| BLE Service | 1 | ✅ Completo |
| WiFi Widget | 1 | ✅ Completo |
| System Widget | 1 | ✅ Completo |
| Clock Widget | 1 | ✅ Completo |
| GPIO Manager | 2 | ✅ Completi |
| I2C Manager | 2 | ✅ Completi |
| Dashboard | 1 | ✅ Completo |

**Totale:** 19 esempi completi e ready-to-use

---

## 🎯 Guida Rapida per Tipo di Utente

### 👨‍💻 Sviluppatore Nuovo

**Percorso:**
1. [README_DOCUMENTAZIONE.md](README_DOCUMENTAZIONE.md) - Quick start
2. [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) - Sezioni 1-3
3. [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) - Sezione 7 (Setup)
4. Implementare Core Managers

### 🔧 Sviluppatore Hardware

**Percorso:**
1. [ESTENSIONE_PERIPHERAL_MANAGER.md](ESTENSIONE_PERIPHERAL_MANAGER.md) - Sezioni 2-4
2. [ESTENSIONE_PERIPHERAL_MANAGER.md](ESTENSIONE_PERIPHERAL_MANAGER.md) - Sezione 6 (Esempi)
3. Implementare GPIO/I2C Manager

### 🌐 Sviluppatore IoT/Network

**Percorso:**
1. [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) - Sezione 4 (Services)
2. [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) - Sezione 6 (Thread-Safety)
3. Implementare WiFi/BLE/MQTT Services

### 🎨 Sviluppatore UI/UX

**Percorso:**
1. [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) - Sezione 5 (Widgets)
2. [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) - Sezione 3.1 (Screen Manager)
3. Creare widget e screen custom

### 🐛 Debugger/Tester

**Percorso:**
1. [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) - Sezione 11 (Problemi Comuni)
2. [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) - Sezione 13 (Checklist Pre-Deploy)
3. [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) - Sezione 6 (Thread-Safety)

---

## 🔍 Ricerca Rapida

### Per Argomento

| Cerco | Documento | Sezione |
|-------|-----------|---------|
| **Setup iniziale** | REPORT_COMPLETO | Sezione 7 |
| **Core pinning** | REPORT_COMPLETO | Sezione 2.2 |
| **Mutex LVGL** | REPORT_COMPLETO | Sezione 6.1 |
| **WiFi Service** | REPORT_COMPLETO | Sezione 4.2 |
| **Widget custom** | REPORT_COMPLETO | Sezione 5.1 |
| **GPIO uso** | ESTENSIONE_PERIPHERAL | Sezione 3 |
| **I2C scan** | ESTENSIONE_PERIPHERAL | Sezione 4 |
| **UI freeze** | REPORT_COMPLETO | Sezione 11.1 |
| **Memory leak** | REPORT_COMPLETO | Sezione 11.3 |
| **PlatformIO** | REPORT_COMPLETO | Sezione 8 |

### Per Esempio Codice

| Esempio | Documento | Sezione |
|---------|-----------|---------|
| Screen Manager | REPORT_COMPLETO | 3.1 |
| WiFi Service | REPORT_COMPLETO | 4.2 |
| WiFi Widget | REPORT_COMPLETO | 5.2 |
| GPIO LED | ESTENSIONE_PERIPHERAL | 6.2 |
| I2C Sensor | ESTENSIONE_PERIPHERAL | 6.3 |
| Dashboard | REPORT_COMPLETO | 5.5 |
| Setup completo | REPORT_COMPLETO | 7 |

---

## 📞 Supporto e Contatti

### Problemi Comuni
Consulta [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) Sezione 11

### Community
- ESP32 Forum: https://esp32.com/
- LVGL Forum: https://forum.lvgl.io/

### Documentazione Ufficiale
- LVGL: https://docs.lvgl.io/
- ESP32: https://docs.espressif.com/

---

**Ultima Revisione:** 2025-11-14
**Versione Documentazione:** 2.0.0
**Maintainer:** ESP32 OS Architecture Team
