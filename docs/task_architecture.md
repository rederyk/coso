# Task e UI Architecture

Linea guida rapida per mantenere la UI reattiva su ESP32-S3 (2 core) con LVGL.

## Core affinity
- **Core 1**: dedicato alla UI (task LVGL). Non far girare worker/comms qui.
- **Core 0**: WiFi, BLE, LED, sensori, timers.

I numeri sono centralizzati in `src/core/task_config.h` (stack, priorità, core).

## LVGL e thread-safety
- **Tutte le chiamate LVGL vanno fatte solo nel task UI.**
- Per pubblicare eventi verso la UI da worker/ISR usa `SystemTasks`:
  - `postUiMessage(...)` / `postUiMessageFromISR(...)` per messaggi semplici.
  - `postUiCallback(fn, ctx)` / `postUiCallbackFromISR(...)` per eseguire una callback sul task UI (mutex già preso).
- Il task UI in `main.cpp` drena la coda e chiama `lv_timer_handler()` mentre detiene il mutex `lvgl_mutex`.

## Pattern consigliato
- Worker: raccoglie dati o gestisce I/O → prepara struct/valori → chiama `postUiCallback` → la callback aggiorna la UI.
- Evita `lv_obj_*` direttamente da task di rete/sensore; niente lambda con catture che rimandano riferimenti invalidi.

## Thread-Safety per Risorse Condivise
- **Singleton multi-thread**: Proteggi sempre con mutex (`SemaphoreHandle_t`)
- **Esempio**: `RgbLedManager` usa mutex per proteggere `setState()` da accessi concorrenti
- **Try-lock pattern**: In loop di update/polling usa `xSemaphoreTake(mutex, 0)` per non bloccare
- **Evita deadlock**: Rilascia mutex prima di chiamare funzioni che potrebbero richiederlo

## Performance
- **LVGL task delay**: 5ms per UI fluida (~200 FPS teorici, limitato da display)
- **Priorità**: LVGL (3) > WiFi (1) > LED (0); BLE (5) usa alta priorità solo per stack Bluetooth
- **Jitter UI**: Verifica che nessun task worker giri su Core 1
