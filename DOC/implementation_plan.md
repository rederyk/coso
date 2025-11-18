# Implementation Plan

## Objective
Realizzare il sistema OS-like per ESP32-S3 Display Touch definito nei report, evitando il codice CC BY-NC-SA e partendo da librerie MIT/BSD (LVGL v8.4.0, TFT_eSPI v2.5.43, FT6336U). Il piano privilegia test seriali e documentazione inline per verificare ciascuna fase.

## Toolchain consigliata
- **PlatformIO**: ideale per prototipazione rapida e per mantenere compatibilità con schemi dei report. Offre gestione librerie, configurazione semplice di LVGL/TFT_eSPI e supporto a `monitor` per test seriali; consigliato per il ritmo attuale.  
- **ESP-IDF**: più adatto se servono debug avviato via `idf.py monitor`, configurazioni low-level (es. PSRAM avanzata, componenti custom). Valutare l’uso solo se si richiede un controllo puntuale dello stack FreeRTOS o build ufficiali Espressif.

Per questo progetto consiglio **PlatformIO**, poi eventualmente migrare a ESP-IDF quando l’architettura funziona e si vuole un controllo ultimo sulle componenti IDF-specifiche.

## Deliverables
1. **Foundation Layer**: gestione display/touch, LVGL mutex, timer tick e buffer PSRAM (core 1).  
2. **Service Layer**: task WiFi/BLE/NTP/MQTT su core 0 con priorità definite e serial logging degli stati.  
3. **Application Layer**: Screen Manager, widget e dashboard custom con attraversamenti sicuri (mutex/event router).  
4. **Documentazione e serial testing**: report di memoria, core pinning e stato servizi via `Serial.printf`.


## Task Breakdown

### 1. Setup e configurazione
- **Descrizione**: creare progetto PlatformIO, importare LVGL/TFT_eSPI/FT6336U e configurare PSRAM/dma.  
- **Output**: `platformio.ini` con le dipendenze e definizioni PSRAM; `lib_deps` per le librerie MIT/BSD; struttura folder `src/`, `include/`.  
- **Test seriale**: stampare la versione delle librerie all’avvio e verificare che il monitor seriale riporta "LVGL init OK" e buffer allocato in PSRAM.

### 2. HAL/GUI core task
- **Descrizione**: inizializzare LVGL (buf PSRAM, display driver, lvgl mutex) su core 1, creare task `lvgl_task` (8k stack) con `lv_tick` timer.  
- **Test seriale**: loggare `mutex created`, `lv_tick timer started`, `GUI task running` e stampare periodicamente `lv_mem_monitor`.  
- **Note**: tutte le funzioni che aggiornano UI dovranno acquisire il mutex globale (`xSemaphoreTake`/`Give`). Annotare questi pattern nel codice per facilitare eventuale revisione.

### 3. Service Layer e core pinning
- **Descrizione**: implementare `ServiceManager` (registry) con WiFi/BLE/NTP tasks su core 0; ciascun task usa priorità 5-6 ed emette stato su seriale ogni 500ms e al cambio.  
- **Test seriale**: log `ServiceManager` registry, `WiFi status: CONNECTED`, `BLE advertising`, `NTP sync ok`; verificare che i messaggi arrivino mentre LVGL continua a girare. Confermare che i servizi aggiornano la GUI sempre tramite mutex (es. `display_status_label` aggiornandolo solo dopo `xSemaphoreTake`).  
- **Note**: Documentare eventuali errori di init nelle stampe seriale e definire fallback (es. retry wifi/o timeout).  

### 4. Screen & Widget Manager
- **Descrizione**: definire `Screen`/`ScreenManager` con stack di schermo, callback `onCreate/onDestroy`, `Widget` registry e transizioni animate LVGL.  
- **Test seriale**: log `ScreenManager: pushed Dashboard`, `Screen swapped to Settings`, `LVGL mem used X%`. Fornire comandi seriali (es. input su `Serial` o button) per simulare transizioni.  
- **Note**: la transizione deve cancellare le vecchie risorse per prevenire leak (chiudere LVGL objects, delete, call `lv_mem_monitor`).  

### 5. Documentazione e test seriale
- **Descrizione**: inserire commenti essenziali, scrivere README/ANALISI su licensing, includere note su mutex/event router. Documentare la sequenza di test seriali (startup, servizi, screen change).  
- **Test**: script `pio device monitor` per verificare i log, oppure una tastiera virtuale via Serial (es. `t` per test display refresh).  
- **Note**: Allegare i riferimenti a documenti già forniti (report e analisi licenze) per ricordare le regole di riuso e le restrizioni CC BY-NC-SA.


## Testing Strategy
- Tutti i task devono stampare il proprio stato su `Serial` con prefisso chiaro (es. `[Service][WiFi]`, `[LVGL]`).  
- Monitor seriale verifica:  
  1. LVGL e mutex inizializzati  
  2. Task pinned (stringhe `wifi_service on core 0`)  
  3. Widget/screen transitions con mem monitor  
  4. Event router da servizi a GUI (test: invia stringa `s` via serial per forzare aggiornamento label)  
- Usare `pio device monitor -b 115200` per osservare i log e verificare latenza sotto 10ms per il loop GUI (può essere simulato con `pdMS_TO_TICKS` invariato).

## Licensing Notes
- Reimplementare l’architettura con codice originale basato sui report; evitare qualsiasi copia diretta dai tutorial Freenove (CC BY-NC-SA).  
- Includere nei file principali una sezione commentata con i copyright / licenze MIT/BSD delle librerie usate (LVGL, TFT_eSPI, FT6336U).  
- Non usare ESP32-audioI2S (GPL-3.0) se si desidera mantenere licensing permissivo.

## Next Steps
1. Creare scaffolding PlatformIO e configurare PSRAM + librerie.  
2. Implementare task LVGL e service manager, includendo serial logging.  
3. Sviluppare Screen Manager e widget con serial test plan.  
4. Finalizzare documentazione e procedere ai test seriali descritti sopra.  
