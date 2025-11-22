# Analisi: Buffer LVGL Selezionabile a Runtime

**Data:** 2025-11-22
**Riferimento:** [lvgl_buffer_analysis.md](lvgl_buffer_analysis.md)

## Domanda
È possibile implementare le opzioni B (Single Buffer RAM) e C (Double Buffer RAM) selezionabili nelle impostazioni?

---

## TL;DR: Risposta Breve

**❌ NO a runtime** - Impossibile cambiare buffer LVGL dopo `setup()`
**✅ SÌ a compile-time** - Possibile selezionare tramite define/config prima del boot
**⚠️ FORSE con reboot** - Teoricamente possibile salvare setting e riavviare

**Complessità:** 🔴 **MOLTO ALTA** (non consigliato)
**Alternativa consigliata:** Scegliere una configurazione ottimale e mantenerla fissa

---

## 1. Analisi Tecnica delle Limitazioni

### 1.1 Inizializzazione LVGL (Non Reversibile)

**File:** [src/main.cpp:286-313](../src/main.cpp#L286-L313)

```cpp
void setup() {
    // ...
    lv_init();  // ← Inizializza LVGL (una sola volta!)

    // Alloca buffer
    draw_buf_ptr = allocatePsramBuffer(draw_buf_bytes, "LVGL draw buffer");

    // Inizializza buffer draw (NON può essere cambiato dopo!)
    lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, nullptr, DRAW_BUF_PIXELS);

    // Registra driver display (usa riferimento al draw_buf)
    DisplayManager::getInstance().begin(&tft, &draw_buf, tft_flush_cb);

    // Crea oggetti UI
    // ... centinaia di oggetti LVGL allocati ...
}
```

### 1.2 Problemi Fondamentali

#### ❌ Problema 1: Buffer Hardcoded in Driver Display

Una volta chiamata `lv_disp_draw_buf_init()`, LVGL **copia i puntatori** ai buffer internamente:

```c
// Codice interno LVGL (lv_disp.c)
void lv_disp_draw_buf_init(lv_disp_draw_buf_t * draw_buf, void * buf1, void * buf2, uint32_t size_in_px_cnt) {
    draw_buf->buf1 = buf1;  // ← Salvato internamente
    draw_buf->buf2 = buf2;  // ← Salvato internamente (o NULL per single)
    draw_buf->size = size_in_px_cnt;
    // ... altri campi ...
}
```

**Conseguenza:** Cambiare `draw_buf_ptr` dopo l'init **non ha effetto** - LVGL continua a usare i vecchi puntatori.

#### ❌ Problema 2: Oggetti UI Già Allocati

Dopo `setup()`, esistono decine/centinaia di oggetti LVGL:
- Schermate (`lv_obj_t*`)
- Widget (pulsanti, label, slider, ecc.)
- Style allocati
- Timer LVGL
- Font caricati

**Conseguenza:** Cambiare il buffer **senza** ri-inizializzare tutti gli oggetti causa:
- **Memory leak:** i vecchi oggetti rimangono in memoria
- **Corruption:** riferimenti invalidi tra oggetti
- **Crash:** accesso a memoria liberata

#### ❌ Problema 3: Mutex e Task LVGL Attivi

Il sistema ha task FreeRTOS che accedono a LVGL:
- UI task (Core 1, priorità 3)
- Callback da WiFi/BLE (tramite `SystemTasks`)
- Timer hardware per `lv_tick_inc()`

**Conseguenza:** Fermare LVGL per cambiare buffer richiede:
1. Sospendere **tutti** i task che usano LVGL
2. Drenare tutte le queue di messaggi UI
3. Aspettare che il DMA finisca
4. Ri-inizializzare tutto
5. Riavviare i task

---

## 2. Opzioni di Implementazione

### Opzione A: ❌ Runtime Switching (SCONSIGLIATO)

**Idea:** Aggiungere una funzione `switchBufferMode()` callable dalle Settings.

**Implementazione teorica:**

```cpp
enum class BufferMode {
    PSRAM_SINGLE,     // Opzione A (attuale)
    DRAM_SINGLE,      // Opzione C
    DRAM_DOUBLE       // Opzione B
};

void switchBufferMode(BufferMode new_mode) {
    // 1. Sospendi task UI
    vTaskSuspend(ui_task_handle);

    // 2. Lock mutex LVGL
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

    // 3. Aspetta DMA
    tft.dmaWait();

    // 4. Distruggi TUTTI gli oggetti UI (⚠️ complesso!)
    lv_obj_clean(lv_scr_act());
    // ... distruggi overlay, launcher, ecc...

    // 5. De-registra driver display (⚠️ non esiste API pubblica!)
    // PROBLEMA: LVGL non offre lv_disp_drv_unregister()!

    // 6. Libera vecchi buffer
    if (draw_buf_ptr) heap_caps_free(draw_buf_ptr);
    if (draw_buf_ptr2) heap_caps_free(draw_buf_ptr2);

    // 7. Alloca nuovi buffer
    switch (new_mode) {
        case DRAM_SINGLE:
            draw_buf_ptr = heap_caps_malloc(..., MALLOC_CAP_INTERNAL);
            lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, nullptr, ...);
            break;
        case DRAM_DOUBLE:
            draw_buf_ptr = heap_caps_malloc(..., MALLOC_CAP_INTERNAL);
            draw_buf_ptr2 = heap_caps_malloc(..., MALLOC_CAP_INTERNAL);
            lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, draw_buf_ptr2, ...);
            break;
    }

    // 8. Ri-crea TUTTI gli oggetti UI (⚠️ MOLTO complesso!)
    createAllScreensAgain();
    recreateOverlayLayer();
    recreateLauncherLayer();
    recreateDockWidget();
    // ... centinaia di righe di codice duplicato ...

    // 9. Unlock mutex e riprendi task
    xSemaphoreGive(lvgl_mutex);
    vTaskResume(ui_task_handle);
}
```

**Problemi critici:**

1. **LVGL non supporta de-registrazione driver**
   - Non esiste `lv_disp_drv_unregister()` in LVGL 8.x
   - Bisognerebbe patchare LVGL stesso

2. **Ri-creazione UI impossibile senza duplicazione codice**
   - Ogni schermata ha logica di creazione in `Screen::show()`
   - Bisognerebbe estrarre tutto in funzioni riutilizzabili
   - **Effort:** settimane di refactoring

3. **Race condition inevitabili**
   - Messaggi in volo da WiFi/BLE task
   - Timer che scattano durante lo switch
   - Callback DMA pending

4. **Frammentazione memoria**
   - Allocare/deallocare buffer da 15-30 KB ripetutamente
   - Rischio di OOM dopo N switch

**Verdict:** ❌ **NON FATTIBILE** senza riscrivere metà del progetto.

---

### Opzione B: ⚠️ Reboot-Based Switching (POSSIBILE ma COSTOSO)

**Idea:** Salvare la preferenza nelle settings e riavviare l'ESP32.

**Implementazione:**

```cpp
// 1. Aggiungi setting
struct SettingsSnapshot {
    // ...
    BufferMode lvglBufferMode = BufferMode::DRAM_SINGLE;  // ← Nuovo campo
};

// 2. In setup(), leggi la preferenza
void setup() {
    // ...
    SettingsManager& settings = SettingsManager::getInstance();
    settings.begin();

    BufferMode mode = settings.getLvglBufferMode();

    lv_init();

    // Alloca buffer in base al setting
    switch (mode) {
        case BufferMode::PSRAM_SINGLE:
            draw_buf_ptr = allocatePsramBuffer(...);
            lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, nullptr, ...);
            break;

        case BufferMode::DRAM_SINGLE:
            draw_buf_ptr = heap_caps_malloc(..., MALLOC_CAP_INTERNAL);
            lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, nullptr, ...);
            break;

        case BufferMode::DRAM_DOUBLE:
            draw_buf_ptr = heap_caps_malloc(..., MALLOC_CAP_INTERNAL);
            draw_buf_ptr2 = heap_caps_malloc(..., MALLOC_CAP_INTERNAL);
            lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, draw_buf_ptr2, ...);
            break;
    }
    // ...
}

// 3. Nelle Settings UI, quando si cambia buffer mode:
void onBufferModeChange(BufferMode new_mode) {
    SettingsManager::getInstance().setLvglBufferMode(new_mode);

    // Mostra dialog: "Restart required. Reboot now?"
    showConfirmDialog("Buffer mode will change after restart. Reboot now?", []() {
        ESP.restart();  // ← Reboot
    });
}
```

**✅ Pro:**
- **Tecnicamente semplice:** nessun hack LVGL
- **Affidabile:** boot pulito ogni volta
- **Testabile:** facile verificare ogni modalità

**❌ Contro:**
- **UX pessima:** "Restart required" per ogni cambio setting
- **Perdita stato:** WiFi si disconnette, animazioni si interrompono
- **Tempo:** ~5-10 secondi per riavvio completo
- **Overkill:** riavviare l'intero sistema solo per un buffer

**Complessità implementazione:** 🟡 **Media** (~2-3 ore)

**Verdict:** ⚠️ **FATTIBILE** ma esperienza utente scadente.

---

### Opzione C: ✅ Compile-Time Selection (CONSIGLIATO)

**Idea:** Usare define del preprocessore, selezionabile via `platformio.ini` o menu `sdkconfig`.

**Implementazione:**

```cpp
// platformio.ini
build_flags =
  # ... altri flags ...
  -D LVGL_BUFFER_MODE=DRAM_DOUBLE  # ← Scegliere: PSRAM_SINGLE, DRAM_SINGLE, DRAM_DOUBLE

// src/main.cpp
enum class BufferMode {
    PSRAM_SINGLE = 0,
    DRAM_SINGLE = 1,
    DRAM_DOUBLE = 2
};

#ifndef LVGL_BUFFER_MODE
  #define LVGL_BUFFER_MODE DRAM_SINGLE  // Default
#endif

void setup() {
    lv_init();

    #if LVGL_BUFFER_MODE == PSRAM_SINGLE
        draw_buf_ptr = allocatePsramBuffer(draw_buf_bytes, "LVGL");
        lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, nullptr, DRAW_BUF_PIXELS);
        Logger::getInstance().info("[LVGL] Buffer: PSRAM Single");

    #elif LVGL_BUFFER_MODE == DRAM_SINGLE
        draw_buf_ptr = heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL);
        lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, nullptr, DRAW_BUF_PIXELS);
        Logger::getInstance().info("[LVGL] Buffer: DRAM Single");

    #elif LVGL_BUFFER_MODE == DRAM_DOUBLE
        draw_buf_ptr = heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL);
        draw_buf_ptr2 = heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL);
        lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, draw_buf_ptr2, DRAW_BUF_PIXELS);
        Logger::getInstance().info("[LVGL] Buffer: DRAM Double");
    #endif

    // ...
}
```

**Uso:**
```bash
# Per testare DRAM single:
pio run -e freenove-esp32-s3-display -t upload --build-flag="-D LVGL_BUFFER_MODE=1"

# Per testare DRAM double:
pio run -e freenove-esp32-s3-display -t upload --build-flag="-D LVGL_BUFFER_MODE=2"
```

**✅ Pro:**
- **Zero overhead runtime:** nessun check, nessun branch
- **Sicuro:** compilatore elimina codice non usato
- **Facile testare:** cambi flag e ri-flash
- **No reboot UI:** cambio solo durante sviluppo/testing

**❌ Contro:**
- **Non selezionabile dall'utente finale** (ma è davvero necessario?)
- **Richiede re-flash** per cambiare (una tantum)

**Complessità implementazione:** 🟢 **Bassa** (~30 minuti)

**Verdict:** ✅ **IDEALE per sviluppo e testing**.

---

## 3. Analisi Costi-Benefici

### Scenario: Utente Finale Vuole Cambiare Buffer

**Domanda:** Quanto è realistico che un utente voglia switchare tra buffer modes?

**Considerazioni:**

1. **Utente tecnico (Developer Mode):**
   - Sa cosa significa "double buffering"
   - Ha accesso a PlatformIO
   - **Soluzione:** Usa compile-time flag (Opzione C)

2. **Utente normale:**
   - Non capisce differenza tra PSRAM e DRAM
   - Non nota differenza di performance (UI già fluida)
   - **Soluzione:** Scegli configurazione ottimale di default

3. **Power user (vuole massime performance):**
   - Potrebbe apprezzare "Performance Mode" toggle
   - **Soluzione:** Opzione B (reboot-based), ma con UX migliorata:
     - "Performance Mode: [Off] Normal → [On] Maximum (requires restart)"
     - Sotto: "Maximum uses 30 KB RAM for smoother animations"

### Raccomandazione per Power User

Se **davvero** vuoi offrire scelta all'utente finale:

**Implementa solo 2 modalità:**
- **Standard Mode** (DRAM Single) - Default
- **Performance Mode** (DRAM Double) - Richiede reboot

**UI nelle Settings:**

```
┌─────────────────────────────────────┐
│ Display Settings                    │
├─────────────────────────────────────┤
│                                     │
│ Performance Mode        [ ] Off     │
│                         [•] Maximum │
│                                     │
│ ⓘ Maximum mode uses double          │
│   buffering for smoother animations │
│   (requires 30 KB RAM + restart)    │
│                                     │
│ [Apply & Restart]                   │
│                                     │
└─────────────────────────────────────┘
```

**Implementazione:** Opzione B (2-3 ore di lavoro)

---

## 4. Conclusioni e Raccomandazioni

### 🎯 Per Questo Progetto

| Obiettivo | Soluzione Consigliata | Complessità |
|-----------|----------------------|-------------|
| **Testing/Debug** | Opzione C (Compile-time) | 🟢 Bassa |
| **Produzione (utenti normali)** | Scegli DRAM Single (hardcoded) | 🟢 Triviale |
| **Produzione (power users)** | Opzione B (Standard/Performance + reboot) | 🟡 Media |
| **Runtime switching senza reboot** | ❌ Non implementare | 🔴 Impossibile |

### ✅ Raccomandazione Finale

**Step 1:** Implementa **Opzione C** (compile-time) per testing:
- Effort: 30 minuti
- Beneficio: Puoi testare tutte le modalità facilmente
- Codice pulito con `#if/#elif`

**Step 2:** Testa le performance con il benchmark fornito in [lvgl_buffer_analysis.md](lvgl_buffer_analysis.md)

**Step 3:** Scegli modalità ottimale in base ai risultati:
- Se DRAM Single è sufficiente → **hardcoda** quello
- Se serve Double Buffer → **hardcoda** quello
- Se vuoi offrire scelta → implementa **Opzione B** (Standard/Performance con reboot)

**Step 4:** Rimuovi le `#if/#elif` e lascia solo il codice della modalità scelta

### ❌ Cosa NON Fare

- Non tentare runtime switching (Opzione A) → spreco di tempo
- Non offrire 3+ opzioni all'utente → confusione
- Non complicare senza misurare → prima i benchmark, poi decidi

---

## 5. Codice di Esempio (Opzione C - Compile Time)

### File: `src/main.cpp`

```cpp
// ========== CONFIGURAZIONE BUFFER LVGL ==========
// Modifica questa define per testare diverse modalità:
//   0 = PSRAM Single Buffer (baseline)
//   1 = DRAM Single Buffer (raccomandato)
//   2 = DRAM Double Buffer (massime performance)
#ifndef LVGL_BUFFER_MODE
  #define LVGL_BUFFER_MODE 1  // Default: DRAM Single
#endif

static lv_color_t* draw_buf_ptr = nullptr;
#if LVGL_BUFFER_MODE == 2  // Double buffer
  static lv_color_t* draw_buf_ptr2 = nullptr;
#endif

// In setup():
void setup() {
    // ... init precedenti ...

    lv_init();
    logMemoryStats("After lv_init");

    const size_t draw_buf_bytes = DRAW_BUF_PIXELS * sizeof(lv_color_t);

    #if LVGL_BUFFER_MODE == 0
        // PSRAM Single Buffer
        draw_buf_ptr = static_cast<lv_color_t*>(
            allocatePsramBuffer(draw_buf_bytes, "LVGL draw buffer")
        );
        if (!draw_buf_ptr) {
            logger.error("[LVGL] Failed to allocate PSRAM buffer");
            while(1) vTaskDelay(portMAX_DELAY);
        }
        lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, nullptr, DRAW_BUF_PIXELS);
        logger.info("[LVGL] Mode: PSRAM Single Buffer");

    #elif LVGL_BUFFER_MODE == 1
        // DRAM Single Buffer (RACCOMANDATO)
        draw_buf_ptr = static_cast<lv_color_t*>(
            heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
        );
        if (!draw_buf_ptr) {
            logger.error("[LVGL] Failed to allocate DRAM buffer");
            while(1) vTaskDelay(portMAX_DELAY);
        }
        lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, nullptr, DRAW_BUF_PIXELS);
        logger.infof("[LVGL] Mode: DRAM Single Buffer (%u bytes @ %p)",
                     draw_buf_bytes, draw_buf_ptr);

    #elif LVGL_BUFFER_MODE == 2
        // DRAM Double Buffer (MASSIME PERFORMANCE)
        draw_buf_ptr = static_cast<lv_color_t*>(
            heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
        );
        draw_buf_ptr2 = static_cast<lv_color_t*>(
            heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
        );
        if (!draw_buf_ptr || !draw_buf_ptr2) {
            logger.error("[LVGL] Failed to allocate double buffers");
            while(1) vTaskDelay(portMAX_DELAY);
        }
        lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, draw_buf_ptr2, DRAW_BUF_PIXELS);
        logger.infof("[LVGL] Mode: DRAM Double Buffer (%u x 2 bytes)", draw_buf_bytes);

    #else
        #error "LVGL_BUFFER_MODE must be 0, 1, or 2"
    #endif

    logMemoryStats("After draw buffer");
    // ...
}
```

### Uso da Command Line

```bash
# Test PSRAM Single
pio run -t upload --build-flag="-D LVGL_BUFFER_MODE=0"

# Test DRAM Single
pio run -t upload --build-flag="-D LVGL_BUFFER_MODE=1"

# Test DRAM Double
pio run -t upload --build-flag="-D LVGL_BUFFER_MODE=2"
```

### Oppure in `platformio.ini`

```ini
[env:freenove-esp32-s3-display]
platform = espressif32
# ...
build_flags =
  # ... altri flags ...
  -D LVGL_BUFFER_MODE=1  # ← Cambia qui per testare
```

---

## 6. Riferimenti

- **Documento principale:** [lvgl_buffer_analysis.md](lvgl_buffer_analysis.md)
- **LVGL Docs:** [Display driver](https://docs.lvgl.io/8.3/porting/display.html)
- **ESP-IDF Heap:** [heap_caps_malloc](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/mem_alloc.html)

---

**Ultimo aggiornamento:** 2025-11-22
**Autore:** Claude Code Analysis
**Status:** ✅ Analysis complete - Opzione C consigliata
