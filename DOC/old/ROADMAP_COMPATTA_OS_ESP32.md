# Roadmap Compatta – ESP32-S3 Touch OS

Documento sintetico pensato per guidare l'implementazione senza dover consultare l'intero corpus in `DOC/`, basato sullo stato attuale (versione 0.5.0, core managers e dashboard operativi).

## Punto di partenza (completato)
- Stack hardware validato: Freenove ESP32-S3 Display 2.8" con ILI9341 + FT6336U, PSRAM attiva.
- LVGL 8.4 + driver TFT_eSPI e touch integrati, task LVGL dedicato su Core 1 con mutex globale.
- Screen/App/Event managers, dashboard screen con widget Clock e SystemInfo, Settings/Info screen stub.

## Fase A – Stabilizzazione Core (Priorità 0)
1. **SettingsManager + Storage**
   - Creare manager con Preferences (NVS) e API type-safe.
   - Integrare in SettingsScreen per WiFi credentials, brightness e meta info.
2. **Ottimizzazione PSRAM (rif. `OTTIMIZZAZIONE_PSRAM_LVGL.md`)**
   - Abilitare allocator LVGL custom su PSRAM e doppio draw buffer.
   - Esporre utility `ui_alloc()` / `ui_free()` e monitor PSRAM in SystemInfoWidget.
3. **Info/Settings Screen**
   - Popolare InfoScreen con dati runtime (chip, flash, versione) e completare UI Settings con binding al manager.

## Fase B – Service Layer (Priorità 1)
1. **Service Framework**
   - Introdurre classe base Service + ServiceManager (start/stop, state tracking).
2. **WiFiService**
   - Gestione STA mode, stato connessione e callback; storage credenziali via SettingsManager.
3. **WiFiStatusWidget**
   - Visualizzare stato, RSSI e IP con colori di stato; aggiornare dashboard.
4. **Preparare hook per BLE/NTP (facoltativo)**
   - Stub di servizio con API coerenti per estensioni future.

## Fase C – Feature Avanzate (Priorità 2)
1. **Widget Avanzati**
   - BatteryWidget (se hardware disponibile) e ChartWidget per log dati (buffer in PSRAM).
2. **Peripheral Manager Layer**
   - Base class, GPIO manager con ownership tracking, I2C manager con scanner e hot-plug; predisporre registry unico.
3. **Asset/Theme foundations**
   - Definire struttura file (SPIFFS/LittleFS) e placeholder per Theme/Asset manager se necessario per release successiva.

## Fase D – Hardening e Release (Priorità 3)
1. **Testing & Profiling**
   - Stress LVGL (double buffer), memory leak detection continuativa, verifica core pinning/touch responsiveness.
2. **Ottimizzazione performance**
   - DMA SPI, taratura refresh, logging selettivo.
3. **Documentazione & Demo**
   - Aggiornare `README.md`, commenti Doxygen essenziali, checklist demo (dashboard con WiFi live, settings persistenti).

## Dipendenze e note operative
- Le fasi di Service Layer dipendono da SettingsManager funzionante.
- L’ottimizzazione PSRAM va completata prima di introdurre nuovi widget per evitare saturazione DRAM.
- Peripheral Manager è opzionale ma raccomandato prima di integrare sensori/attuatori aggiuntivi.
- Ogni blocco va chiuso con piccola sessione di test (build + upload + smoke test touch/UI) per evitare regressioni accumulate.
