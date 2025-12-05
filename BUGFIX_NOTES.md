# LVGL Power Manager - Bugfix Notes

## Problema Iniziale
Il dispositivo si bloccava durante l'inizializzazione con schermo nero.

## Cause Identificate

### 1. **lv_mem_monitor() con Custom Allocator**
- `lv_mem_monitor()` non funziona con custom PSRAM allocator
- Causava crash in `cleanLVGLCaches()`
- **Fix**: Rimosso lv_mem_monitor, cleanup manuale

### 2. **Mutex LVGL Mancante**
- `suspend()` e `resume()` modificavano stato LVGL senza lock
- Poteva causare race condition con LVGL task
- **Fix**: Aggiunto `lvgl_mutex_lock()` / `lvgl_mutex_unlock()`

### 3. **Auto-Suspend da Loop Principale**
- `update()` chiamato da `loop()` (diverso task da LVGL)
- Chiamare `suspend()` cross-task causava deadlock
- **Fix**: Auto-suspend **DISABILITATO** per ora

## Stato Attuale

### ✅ **Funzionante**
- Ottimizzazioni lv_conf.h (~20KB salvati)
- Sistema suspend/resume con mutex protection
- Memory monitoring
- Touch detection + reset idle timer

### ⚠️ **Modalità Manuale Only**
```cpp
// AUTO-SUSPEND DISABLED
// Chiamare manualmente:
LVGLPowerMgr.switchToVoiceMode();  // Sospende LVGL
LVGLPowerMgr.switchToUIMode();      // Riprende LVGL
```

### ❌ **Non Funzionante**
- Auto-suspend dopo timeout (causa freezes)

## Soluzioni Future

### Opzione A: Timer LVGL per Auto-Suspend
```cpp
// In setup(), DOPO aver inizializzato LVGL:
static lv_timer_t* auto_suspend_timer = lv_timer_create([](lv_timer_t* t) {
    // Questo gira nel task LVGL, quindi è sicuro
    if (idle_time > threshold) {
        LVGLPowerMgr.suspend();  // Già nel task LVGL, no deadlock
    }
}, 5000, nullptr);  // Check ogni 5 secondi
```

### Opzione B: Queue Message System
```cpp
// Da loop():
if (should_suspend) {
    SystemTasks::postUiMessage(UiMessageType::SuspendLVGL);
}

// Nel task LVGL:
case UiMessageType::SuspendLVGL:
    LVGLPowerMgr.suspend();  // Sicuro, siamo nel task LVGL
    break;
```

### Opzione C: Disable LVGL Task Temporarily
```cpp
// Sospendi il task LVGL
vTaskSuspend(lvgl_task_handle);

// Poi fai cleanup
pauseLVGLTimers();
cleanLVGLCaches();

// Resume quando serve
vTaskResume(lvgl_task_handle);
```

## Utilizzo Corrente

### Inizializzazione
```cpp
LVGLPowerMgr.init();
LVGLPowerMgr.setAutoSuspendEnabled(false);  // MUST BE FALSE
```

### Cambio Modalità Manuale (Da UI, quindi sicuro)
```cpp
// In un event handler LVGL:
static void voice_button_clicked(lv_event_t* e) {
    // Siamo già nel task LVGL, quindi sicuro
    LVGLPowerMgr.switchToVoiceMode();
}

static void ui_button_clicked(lv_event_t* e) {
    LVGLPowerMgr.switchToUIMode();
}
```

### Memory Stats
```cpp
LVGLPowerMgr.printMemoryStats();  // Sicuro, solo logging
```

## Testing

### Test 1: Ottimizzazioni Memoria
```
Confronta memoria all'avvio:
- Prima: ~163KB DRAM libera
- Dopo: ~183KB DRAM libera (+20KB) ✅
```

### Test 2: Suspend Manuale
```cpp
// In developer_screen o settings_screen, aggiungere pulsante:
lv_obj_t* suspend_btn = lv_btn_create(parent);
lv_obj_add_event_cb(suspend_btn, [](lv_event_t* e) {
    LVGLPowerMgr.switchToVoiceMode();
    ESP_LOGI("TEST", "LVGL suspended, check DRAM!");
}, LV_EVENT_CLICKED, nullptr);
```

### Test 3: Resume da Touch
```
1. Sospendi LVGL (schermo nero)
2. Tocca schermo
3. UI dovrebbe riprendere ✅
```

## Logs Attesi

### Boot OK:
```
[LVGLPower] Initializing LVGL Power Manager
[LVGLPower] LVGL Power Manager initialized
[LVGLPower] Power manager ready - manual mode only
[LVGLPower] Auto-suspend DISABLED - use manual switchToVoiceMode() instead
[LVGLPowerMgr] === Memory Stats ===
[LVGLPowerMgr] DRAM:  183000 / 324000 KB free (56.4%)
[LVGLPowerMgr] PSRAM: 7800000 / 8000000 KB free (97.5%)
[LVGLPowerMgr] Mode: UI, LVGL State: ACTIVE
```

### Suspend OK:
```
[LVGLPowerMgr] Suspending LVGL...
[LVGLPowerMgr] Pausing LVGL timers
[LVGLPowerMgr] Cleaning LVGL caches
[LVGLPowerMgr] Cache cleanup complete
[LVGLPowerMgr] LVGL suspended. Freed ~XX KB DRAM
[LVGLPowerMgr] DRAM:  265000 / 324000 KB free (81.7%)  ← +80KB!
```

### Resume OK:
```
[Touch] Touch detected
[LVGLPowerMgr] Touch detected
[LVGLPowerMgr] Resuming LVGL...
[LVGLPowerMgr] Resuming LVGL timers
[LVGLPowerMgr] LVGL resumed
```

## Raccomandazioni

1. ✅ **Usa solo modalità manuale** per ora
2. ✅ **Testa memory savings** prima di implementare auto-suspend
3. ⚠️ **Non abilitare auto-suspend** senza riscrivere con timer LVGL
4. ✅ **Aggiungi pulsanti UI** per testare suspend/resume
5. ✅ **Monitora logs** per verificare che non ci siano deadlock

## Performance Attese

| Scenario | DRAM Libera | Note |
|----------|-------------|------|
| Boot (before) | ~163KB | Baseline |
| Boot (after opt) | ~183KB | +20KB dalle ottimizzazioni |
| UI Active | ~183KB | Normale operazione |
| LVGL Suspended | ~265-285KB | +80-100KB, voice assistant può partire |
| Hybrid Mode | ~200-220KB | Se abbastanza RAM disponibile |

## Sicurezza Thread

✅ **Safe Operations** (da qualsiasi thread):
- `init()`
- `getFreeDRAM()` / `getFreePSRAM()`
- `printMemoryStats()`
- `getCurrentMode()` / `getLVGLState()`
- `resetIdleTimer()`

⚠️ **Needs LVGL Task Context**:
- `suspend()` - usa mutex, ma meglio da LVGL task
- `resume()` - usa mutex, ma meglio da LVGL task
- `switchToVoiceMode()` - chiama suspend()
- `switchToUIMode()` - chiama resume()

❌ **Not Safe from loop()**:
- `update()` con auto-suspend enabled
