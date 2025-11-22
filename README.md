# ESP32-S3 Touch Display OS - Provisional

Firmware avanzato per dispositivo ESP32-S3 con display touch capacitivo e gestione periferiche integrate, progettato come base per un assistente vocale open-source simile ad Alexa.

## Descrizione del Progetto

Questo firmware trasforma un ESP32-S3 con display touch 2.8" (320×240) in un'interfaccia intelligente per controllo dispositivi e interazione utente. Include gestione avanzata delle risorse, thread-safety, e architettura modulare per espandibilità.

### Caratteristiche Principali
- **Interfaccia Touch**: LVGL basato con tema personalizzabile e dock navigabile
- **Connettività**: WiFi, BLE 5.0 con ruoli server/client, HID wireless
- **Periferiche**: Display RGB LED, SD card, sensori touch capacitivi
- **Sistema Operativo**: Architettura RTOS con gestione thread avanzata e core affinity
- **Memoria Avanzata**: Supporto PSRAM con allocazione ottimizzata per LVGL
- **Sicurezza**: Salvataggio atomico configurazioni con backup auto

## Licenza

Apache-2.0. Vedi `LICENSE` e `NOTICE` per dettagli e attribuzioni di terze parti.

## 🚀 Quick Start

```bash
platformio run -t upload
platformio device monitor
```

## ⚙️ Configurazione Buffer LVGL

Il progetto supporta **3 modalità di buffer** per ottimizzare le performance LVGL.

**Modifica `platformio.ini` linea 43:**

```ini
build_flags =
  -D LVGL_BUFFER_MODE=1  # 0=PSRAM, 1=DRAM Single, 2=DRAM Double
```

| Modo | Buffer Type | RAM Used | Performance | Raccomandato |
|------|-------------|----------|-------------|-------------|
| 0    | PSRAM Single | 0 KB DRAM | Media | <50KB RAM libera |
| 1    | DRAM Single  | 15 KB DRAM | Alta | Bilanciato |
| 2    | DRAM Double  | 30 KB DRAM | Ottima | Max performance |

## 📚 Documentazione Disponibile
- **[QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md)** - Guida buffer LVGL
- **[task_architecture.md](docs/task_architecture.md)** - Architettura thread-safe
- **[lvgl_buffer_analysis.md](docs/lvgl_buffer_analysis.md)** - Analisi tecnica
- **[MEMORY_USAGE_AND_OPTIMIZATION.md](docs/MEMORY_USAGE_AND_OPTIMIZATION.md)** - Ottimizzazioni

## 📈 Roadmap (Senza Date/Tempi)

### Fase 1: Interfaccia e Controlli Base ✅
- Implementare dock navigabile e schermate applicazione
- Aggiungere controlli per luminosità, tema, orientamento
- Integrazione sensori touch e feedback RGB LED

### Fase 2: Connettività Avanzata ✅
- Estendere BLE per ruoli client/server con servizi custom
- Implementare Command Center per esecuzione remota comandi
- Aggiungere web server con interfaccia file manager

### Fase 3: Multimedia e GPIO
- Integrare player audio/video per SD card
- Espandere supporto GPIO per controlli diretti dispositivi
- Ottimizzare codec video per display limitato

### Fase 4: AI e Sicurezza
- Integrazione server MCP per comunicazione esterna
- Aggiungere supporto elaborazione AI locale/remota
- Migliorare crittografia e autenticazione wireless

### Fase Finale: Assistente Vocale Open-Source
- **Obiettivo Finale**: Trasformare il dispositivo in un sostituto open-source di Alexa
  - **IA Avanzata**: Integrazione con API AI attraverso MCP server per processamento intelligente
  - **Moduli Direct GPIO**: Controllo dispositivi domestici con interfaccia diretta agli I/O ESP32
  - **Architettura Modulare**: Framework estensibile per aggiunta abilità NLP, riconoscimento vocale, controllo home automation
  - **Distribuzione**: Progetto completamente open-source con community foothold per evoluzioni collaborative
