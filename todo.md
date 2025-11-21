
## ✅ Modifiche Applicate (2025-11-22)

### Architettura Task - Implementazione Completata

**Riferimento:** `docs/task_architecture.md`

1. **Core Affinity Configurato Correttamente:**
   - Core 1: UI/LVGL task (priorità 3)
   - Core 0: WiFi, BLE, LED, worker tasks (priorità più basse)
   - Configurazione centralizzata in `src/core/task_config.h`

2. **Sistema di Messaggistica UI:**
   - `SystemTasks::postUiMessage()` per notifiche dai worker thread
   - `SystemTasks::postUiCallback()` per eseguire codice sul task UI
   - Queue FreeRTOS thread-safe tra worker e UI

3. **WiFi Manager Aggiornato:**
   - Ora usa `SystemTasks` per notificare la UI dei cambi di stato
   - Eventi: `WifiStatus` (connected/disconnected)
   - Comunicazione thread-safe via message queue

4. **Task Isolati e Thread-Safe:**
   - WiFi task su Core 0 (priorità 1)
   - BLE task su Core 0 (priorità 5) - usa già queue interna
   - LED task su Core 0 (priorità 0)
   - LVGL task su Core 1 (priorità 3) - drena la queue e aggiorna UI

### Ottimizzazioni Thread-Safety e Performance

5. **RgbLedManager Protetto con Mutex:**
   - Aggiunto `SemaphoreHandle_t state_mutex_` per proteggere accessi concorrenti
   - Metodi `setState()`, `setBrightness()`, `setColor()`, `off()` ora thread-safe
   - Metodo `update()` usa try-lock (non blocca) per evitare deadlock
   - **Risolto:** Race condition tra WiFi task e LED task eliminato

6. **LVGL Task Ottimizzato:**
   - Ridotto delay da 10ms a 5ms per UI più fluida
   - Massimo teorico: ~200 FPS (praticamente limitato da display refresh rate)
   - Migliore responsività al tocco e animazioni più smooth

---

## TODO Futuri

### 🎯 Prossima Ottimizzazione Consigliata

**Buffer LVGL in RAM Interna** - ✅ Ready for implementation
- **Riferimento:** `docs/lvgl_buffer_analysis.md`
- **Azione:** Spostare il buffer LVGL (15 KB) da PSRAM a RAM interna
- **Beneficio:** Performance DMA ~2x migliori (40-60 MB/s → 80 MB/s)
- **Costo:** 15 KB di RAM interna (disponibili ~280 KB)
- **Complessità:** ⭐ Triviale (modifica 1 riga in `src/main.cpp:290`)
- **Test:** Benchmark DMA fornito nel documento

### Feature Future

- aggiungiere command center per avviare comandi tramite get instance, per prepararsi a llm mcp

- aggiungiere server web con interfaccia estesa e file manager per caricare e scaricare file nella sd

- aggiungiere player audio video da scheda sd, riduci in automatico la qualità video in base alla risoluzione dello schermo

- continuare a migliorare le prestazioni seguendo le linee guida

---

## Note sull'Architettura (Reference)

### Architettura task (FreeRTOS)

Task UI dedicato, con loop lv_timer_handler() e vTaskDelay(pdMS_TO_TICKS(5-10)); niente altro lavoro lì. Pinna su un core (es. 0) per latenza stabile.
Task I/O/Sensoristica separati (core 1). Raccolgono dati e li pubblicano via queue/message bus verso il task UI, che aggiorna solo lo stato del modello (no chiamate LVGL nei task I/O).
Task comunicazioni (BLE/WiFi) isolato con proprie code; limita callbacks lunghe, rimanda immediatamente su queue al task comms.
Event loop semplice: un’unica queue centrale per UI_EVENT_{DATA_READY,STATE_CHANGE,...} riduce mutex sugli oggetti LVGL.
Ottimizzazioni LVGL

Evita aggiornamenti continui: aggiorna solo quando cambia lo stato; usa lv_obj_invalidate() selettivo.
Riduci complessità UI: meno ombre/blur, meno gradienti, niente animazioni continue (solo timer brevi con easing leggero).
Font: genera un’unica fonte compatta con subset di caratteri; evita 2-3 font grandi caricati in PSRAM.
Buffer display: verifica doppio buffer in RAM interna o singolo buffer + DMA; evita buffer grandi in PSRAM se il driver non fa DMA da PSRAM.
Riduzi immagini: converti in RAW/565 con compressione RLE se supportato; carica asset statici in flash (LittleFS) e copia in RAM solo al bisogno, non in loop.
Limita lv_obj_create dinamici: riusa schermate/oggetti, crea offscreen e mostra con lv_scr_load_anim per transizioni leggere.
Gestione memoria/PSRAM

Mantieni heap interno per stack/task UI e buffer critici; per strutture grandi usa MALLOC_CAP_SPIRAM ma non per il frame buffer DMA (se il driver non supporta).
Evita frammentazione: crea oggetti early al boot, poi non allocare/liberare continuamente. Pool di oggetti o arene per schermate dinamiche.
Parametri utili

Aumenta priorità task UI rispetto agli altri di 1-2 livelli; dimensiona stack (8–12 KB se usi LVGL + JSON parsing).
lv_tick_inc via timer HW a 1 ms; niente delay lunghi nel task UI.
CONFIG_BT_NIMBLE_MAX_CONNECTIONS e bonds già limitati, bene; disattiva log verbosi in release per ridurre jitter.
Prossimi step pratici

Creare file src/system_tasks.cpp con setup dei task (UI, IO, Comms) e code/eventi.
Spostare tutte le chiamate LVGL dentro il task UI; gli altri task inviano solo messaggi.
Configurare driver display per double buffer piccolo in RAM interna; verificare lato driver (Freenove/ESP32S3) se DMA da PSRAM è supportato.
Rigenerare font/asset minimizzati e disabilitare animazioni di default non necessarie.
Se vuoi, posso abbozzare uno scheletro di code per task/queue e ciclo LVGL.