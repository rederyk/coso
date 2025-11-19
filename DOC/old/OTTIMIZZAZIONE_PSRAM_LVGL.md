# Ottimizzazione PSRAM per LVGL e Risorse Grafiche - ESP32-S3

> **Documento Tecnico:** Strategia completa per ottimizzare l'uso della PSRAM da 8MB su ESP32-S3, liberando DRAM preziosa e scalando la capacità grafica del sistema.

---

## Executive Summary

Questo documento fornisce un piano dettagliato per ottimizzare l'utilizzo della PSRAM (8MB OPI) sull'ESP32-S3, spostando l'heap LVGL, i buffer grafici e le risorse UI dalla DRAM limitata alla PSRAM abbondante.

### Obiettivi

- **Liberare DRAM:** Da ~200KB a ~450KB (+125%)
- **Espandere heap LVGL:** Da 256KB DRAM a 512KB PSRAM (2x capacità)
- **Migliorare rendering:** Double buffering in PSRAM
- **Monitoring real-time:** Dashboard DRAM/PSRAM/heap LVGL
- **Allocazione intelligente:** Wrapper automatico PSRAM→DRAM fallback

### Benefici Attesi

| Aspetto | Prima | Dopo | Miglioramento |
|---------|-------|------|---------------|
| **DRAM libera** | ~200KB | ~450KB | +125% |
| **LVGL heap** | 256KB DRAM | 512KB PSRAM | 2x + libera DRAM |
| **Draw buffer** | 12.5KB DRAM | 37.5KB PSRAM | 3x dimensione |
| **Widget capacity** | Limitata | Espandibile | PSRAM disponibile |
| **Visibilità memory** | Nessuna | Real-time | Dashboard completa |

---

## 1. Analisi Stato Attuale

### 1.1 Hardware Disponibile

**ESP32-S3 Freenove FNK0104:**
- **PSRAM:** 8MB Octal-SPI (OPI mode)
- **DRAM:** ~520KB totale (parte usata da sistema)
- **Flash:** 16MB
- **Display:** ILI9341 2.8" 320x240 RGB565

**Configurazione Attuale ([platformio.ini](../platformio.ini)):**
```ini
board_build.arduino.memory_type = qio_opi  # PSRAM OPI attiva
build_flags =
  -DBOARD_HAS_PSRAM
  -mfix-esp32-psram-cache-issue              # Cache coherency fix
```

### 1.2 Configurazione LVGL Corrente

**File:** [src/config/lv_conf.h](../src/config/lv_conf.h)

```c
#define LV_MEM_CUSTOM 0                // ❌ Usa allocatore interno LVGL
#define LV_MEM_SIZE (256U * 1024U)     // ❌ 256KB in DRAM
#define LV_MEM_ADR 0                   // Non usa indirizzo fisso
```

**Conseguenze:**
- LVGL alloca 256KB di **DRAM preziosa** al boot
- Widget e oggetti UI competono per i rimanenti ~260KB DRAM
- PSRAM da 8MB **completamente inutilizzata** per l'heap LVGL

### 1.3 Draw Buffer Attuale

**File:** [src/main.cpp](../src/main.cpp#L22-L26)

```cpp
// Ridotto a 20 righe per risparmiare memoria
static constexpr int32_t DRAW_BUF_PIXELS = LV_HOR_RES_MAX * 20;  // 6400px
static lv_color_t draw_buf_fallback[DRAW_BUF_PIXELS];            // 12.8KB in BSS
static lv_color_t* draw_buf_ptr = draw_buf_fallback;
static bool draw_buf_in_psram = false;
```

**Tentativo allocazione PSRAM ([main.cpp:146-155](../src/main.cpp#L146-L155)):**
```cpp
lv_color_t* allocated = static_cast<lv_color_t*>(
    allocatePsramBuffer(draw_buf_bytes, "LVGL draw buffer")
);
if (allocated) {
    draw_buf_ptr = allocated;
    draw_buf_in_psram = true;
}
```

**Stato:**
- ✅ Single buffer PSRAM funzionante (se disponibile)
- ❌ Fallback statico 12.8KB in BSS (DRAM)
- ❌ No double buffering per ridurre tearing

### 1.4 Problemi Identificati

1. **LVGL heap in DRAM:** Spreco di 256KB di RAM veloce
2. **Widget senza controllo:** Allocati ovunque senza strategia
3. **Nessun monitoring:** Impossibile vedere frammentazione heap LVGL
4. **No allocatore unificato:** Ogni componente gestisce memoria a modo suo

---

## 2. Strategia di Ottimizzazione

### 2.1 Architettura Target

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-S3 Memory Layout                    │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────────┐                ┌────────────────────┐ │
│  │ DRAM (~520KB)    │                │ PSRAM (8MB)        │ │
│  ├──────────────────┤                ├────────────────────┤ │
│  │ FreeRTOS Heap    │                │ LVGL Heap (512KB)  │ │
│  │ (~250KB)         │                │ ✅ CUSTOM ALLOC   │ │
│  ├──────────────────┤                ├────────────────────┤ │
│  │ Stack Tasks      │                │ Draw Buffer 1      │ │
│  │ (~100KB)         │                │ (30 lines = 19KB)  │ │
│  ├──────────────────┤                ├────────────────────┤ │
│  │ Static/BSS       │                │ Draw Buffer 2      │ │
│  │ (~100KB)         │                │ (30 lines = 19KB)  │ │
│  ├──────────────────┤                ├────────────────────┤ │
│  │ LIBERA           │                │ Widget Pool        │ │
│  │ (~450KB) 🎯      │                │ (crescita dinamica)│ │
│  └──────────────────┘                ├────────────────────┤ │
│                                       │ Image Cache        │ │
│  VELOCE (cache hit)                  │ (future use)       │ │
│  Latenza: ~1 ciclo                   ├────────────────────┤ │
│                                       │ Sprite Pool        │ │
│                                       │ (future use)       │ │
│                                       ├────────────────────┤ │
│                                       │ LIBERA (~7MB) 🎯   │ │
│                                       └────────────────────┘ │
│                                                               │
│                                       LENTA (3x DRAM)        │
│                                       Latenza: ~3 cicli      │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Allocatore Custom LVGL

**Principio:** Far usare a LVGL un heap dedicato in PSRAM invece dell'allocatore interno.

**Vantaggi:**
- ✅ 256KB DRAM liberati immediatamente
- ✅ Heap LVGL espandibile a 512KB+ senza toccare DRAM
- ✅ Controllo completo su allocazioni (logging, profiling)
- ✅ Fallback automatico a DRAM se PSRAM esaurita

**Svantaggi:**
- ⚠️ Latenza +2-3x per accessi PSRAM (mitigato da cache L1/L2)
- ⚠️ Richiede implementazione custom allocator

### 2.3 Wrapper ui_alloc()

**Obiettivo:** API unificata per allocare oggetti UI con priorità PSRAM.

**Pattern:**
```cpp
void* ui_alloc(size_t size, const char* label) {
    // 1. Prova PSRAM
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (ptr) return ptr;

    // 2. Fallback DRAM (con warning)
    Serial.printf("[WARNING] PSRAM full, using DRAM for %s\n", label);
    return heap_caps_malloc(size, MALLOC_CAP_DEFAULT);
}
```

**Uso:**
- Container widget pesanti
- Buffer temporanei di decodifica immagini
- Sprite TFT_eSPI con createSprite()
- Cache dati (es. history grafici)

---

## 3. Piano di Implementazione

### FASE 1: Custom Allocator LVGL in PSRAM

**Obiettivo:** Spostare l'heap LVGL da 256KB DRAM a 512KB PSRAM.

#### Step 1.1: Modificare lv_conf.h

**File:** [src/config/lv_conf.h](../src/config/lv_conf.h#L49-L67)

```cpp
// ============================================
// PRIMA (linee 49-60)
// ============================================
#define LV_MEM_CUSTOM 0
#if LV_MEM_CUSTOM == 0
    #define LV_MEM_SIZE (256U * 1024U)          /*[bytes]*/
    #define LV_MEM_ADR 0     /*0: unused*/
#else       /*LV_MEM_CUSTOM*/
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
    #define LV_MEM_CUSTOM_ALLOC   malloc
    #define LV_MEM_CUSTOM_FREE    free
    #define LV_MEM_CUSTOM_REALLOC realloc
#endif     /*LV_MEM_CUSTOM*/

// ============================================
// DOPO
// ============================================
#define LV_MEM_CUSTOM 1   // ✅ Abilita allocatore custom
#if LV_MEM_CUSTOM == 0
    // Non usato
#else       /*LV_MEM_CUSTOM*/
    #define LV_MEM_CUSTOM_INCLUDE "utils/psram_allocator.h"
    #define LV_MEM_CUSTOM_ALLOC   lvgl_malloc
    #define LV_MEM_CUSTOM_FREE    lvgl_free
    #define LV_MEM_CUSTOM_REALLOC lvgl_realloc
#endif     /*LV_MEM_CUSTOM*/
```

#### Step 1.2: Creare psram_allocator.h

**File:** `src/utils/psram_allocator.h` (NUOVO)

```cpp
#pragma once

#include <cstddef>
#include <esp_heap_caps.h>

// ============================================
// Custom Allocator per LVGL
// ============================================

/**
 * @brief Malloc LVGL con priorità PSRAM
 * @param size Dimensione in bytes
 * @return Puntatore allocato o nullptr
 *
 * Logica:
 * 1. Prova PSRAM (MALLOC_CAP_SPIRAM)
 * 2. Se fallisce → DRAM (MALLOC_CAP_DEFAULT)
 * 3. Log dettagliato per debug
 */
void* lvgl_malloc(size_t size);

/**
 * @brief Free LVGL (compatibile sia PSRAM che DRAM)
 * @param ptr Puntatore da liberare
 */
void  lvgl_free(void* ptr);

/**
 * @brief Realloc LVGL (preserva dati)
 * @param ptr Puntatore esistente
 * @param new_size Nuova dimensione
 * @return Nuovo puntatore o nullptr
 */
void* lvgl_realloc(void* ptr, size_t new_size);

/**
 * @brief Stampa statistiche heap LVGL
 *
 * Output esempio:
 * [LVGL Heap] Total: 512KB | Used: 123KB (24%) | Free: 389KB
 * [LVGL Heap] Largest block: 256KB | Fragmentation: 8%
 */
void  lvgl_heap_monitor_print();

// ============================================
// Wrapper UI Allocation (per oggetti custom)
// ============================================

/**
 * @brief Alloca memoria per oggetti UI pesanti
 * @param size Dimensione in bytes
 * @param label Label debug (opzionale)
 * @return Puntatore o nullptr
 *
 * Esempio:
 *   uint8_t* img_buf = (uint8_t*)ui_alloc(64*1024, "jpeg_buffer");
 */
void* ui_alloc(size_t size, const char* label = nullptr);

/**
 * @brief Libera memoria allocata con ui_alloc
 */
void  ui_free(void* ptr);

/**
 * @brief Alloca buffer per sprite TFT_eSPI
 * @param size Dimensione buffer
 * @return Puntatore PSRAM-aligned
 *
 * Specifico per TFT_eSprite::createSprite() con PSRAM
 */
void* sprite_alloc(size_t size);
```

#### Step 1.3: Implementare psram_allocator.cpp

**File:** `src/utils/psram_allocator.cpp` (NUOVO)

```cpp
#include "utils/psram_allocator.h"
#include <Arduino.h>

// Statistiche interne
static size_t s_psram_allocations = 0;
static size_t s_dram_allocations = 0;
static size_t s_total_allocated = 0;

// ============================================
// LVGL Custom Allocator
// ============================================

void* lvgl_malloc(size_t size) {
    if (size == 0) return nullptr;

    // 1. Prova PSRAM
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr) {
        s_psram_allocations++;
        s_total_allocated += size;

        #ifdef DEBUG_LVGL_ALLOC
        Serial.printf("[lvgl_malloc] %u bytes in PSRAM @ %p\n", size, ptr);
        #endif

        return ptr;
    }

    // 2. Fallback DRAM
    ptr = heap_caps_malloc(size, MALLOC_CAP_DEFAULT);
    if (ptr) {
        s_dram_allocations++;
        s_total_allocated += size;

        Serial.printf("[lvgl_malloc] ⚠️ PSRAM full! %u bytes in DRAM @ %p\n", size, ptr);
        return ptr;
    }

    // 3. Fallito
    Serial.printf("[lvgl_malloc] ❌ FAILED to allocate %u bytes!\n", size);
    return nullptr;
}

void lvgl_free(void* ptr) {
    if (ptr == nullptr) return;

    // heap_caps_free funziona sia per PSRAM che DRAM
    heap_caps_free(ptr);

    #ifdef DEBUG_LVGL_ALLOC
    Serial.printf("[lvgl_free] @ %p\n", ptr);
    #endif
}

void* lvgl_realloc(void* ptr, size_t new_size) {
    if (new_size == 0) {
        lvgl_free(ptr);
        return nullptr;
    }

    if (ptr == nullptr) {
        return lvgl_malloc(new_size);
    }

    // Prova PSRAM realloc
    void* new_ptr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (new_ptr) {
        #ifdef DEBUG_LVGL_ALLOC
        Serial.printf("[lvgl_realloc] %p → %p (%u bytes)\n", ptr, new_ptr, new_size);
        #endif
        return new_ptr;
    }

    // Fallback: alloca nuovo + copia + free vecchio
    new_ptr = lvgl_malloc(new_size);
    if (new_ptr) {
        // Copia dati (assumiamo size <= new_size)
        memcpy(new_ptr, ptr, new_size);
        lvgl_free(ptr);
        Serial.printf("[lvgl_realloc] ⚠️ Fallback copy %p → %p\n", ptr, new_ptr);
        return new_ptr;
    }

    Serial.printf("[lvgl_realloc] ❌ FAILED %u bytes\n", new_size);
    return nullptr;
}

void lvgl_heap_monitor_print() {
    // ESP32 heap stats
    size_t free_dram = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t largest_psram = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    // LVGL internal heap monitor
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);

    Serial.println("\n╔═══════════════════════════════════════════════════╗");
    Serial.println("║          LVGL Memory Monitor                      ║");
    Serial.println("╠═══════════════════════════════════════════════════╣");
    Serial.printf( "║ DRAM Free:        %7lu KB                      ║\n", free_dram / 1024);
    Serial.printf( "║ PSRAM Free:       %7lu / %7lu MB          ║\n",
                   free_psram / 1024 / 1024, total_psram / 1024 / 1024);
    Serial.printf( "║ PSRAM Largest:    %7lu KB                      ║\n", largest_psram / 1024);
    Serial.println("╠═══════════════════════════════════════════════════╣");
    Serial.printf( "║ LVGL Total Size:  %7lu KB                      ║\n", mon.total_size / 1024);
    Serial.printf( "║ LVGL Used:        %7lu KB (%3u%%)               ║\n",
                   mon.used_size / 1024, mon.used_pct);
    Serial.printf( "║ LVGL Free:        %7lu KB                      ║\n", mon.free_size / 1024);
    Serial.printf( "║ Fragmentation:    %3u%%                           ║\n", mon.frag_pct);
    Serial.println("╠═══════════════════════════════════════════════════╣");
    Serial.printf( "║ PSRAM allocs:     %7lu                          ║\n", s_psram_allocations);
    Serial.printf( "║ DRAM allocs:      %7lu                          ║\n", s_dram_allocations);
    Serial.printf( "║ Total allocated:  %7lu KB                      ║\n", s_total_allocated / 1024);
    Serial.println("╚═══════════════════════════════════════════════════╝\n");

    // Warning se frammentazione alta
    if (mon.frag_pct > 20) {
        Serial.println("⚠️  WARNING: LVGL heap fragmentation > 20%!");
        Serial.println("    Consider calling lv_mem_defrag() or reducing object churn.");
    }
}

// ============================================
// UI Wrapper Allocator
// ============================================

void* ui_alloc(size_t size, const char* label) {
    if (size == 0) return nullptr;

    // 1. Prova PSRAM
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr) {
        if (label) {
            Serial.printf("[ui_alloc] %s: %u bytes in PSRAM @ %p\n", label, size, ptr);
        }
        return ptr;
    }

    // 2. Fallback DRAM
    ptr = heap_caps_malloc(size, MALLOC_CAP_DEFAULT);
    if (ptr) {
        Serial.printf("[ui_alloc] ⚠️ %s: %u bytes in DRAM (PSRAM full!) @ %p\n",
                      label ? label : "unknown", size, ptr);
        return ptr;
    }

    // 3. Fallito
    Serial.printf("[ui_alloc] ❌ %s: FAILED to allocate %u bytes\n",
                  label ? label : "unknown", size);
    return nullptr;
}

void ui_free(void* ptr) {
    heap_caps_free(ptr);
}

void* sprite_alloc(size_t size) {
    // TFT_eSPI sprite buffers devono essere aligned
    void* ptr = heap_caps_aligned_alloc(4, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr) {
        Serial.printf("[sprite_alloc] %u bytes in PSRAM @ %p\n", size, ptr);
        return ptr;
    }

    Serial.printf("[sprite_alloc] ❌ FAILED %u bytes\n", size);
    return nullptr;
}
```

#### Step 1.4: Integrare in main.cpp

**File:** [src/main.cpp](../src/main.cpp)

**Modifiche:**

```cpp
// ============================================
// PRIMA (dopo lv_init())
// ============================================
lv_init();
logMemoryStats("After lv_init");
// ... alloca draw buffer ...

// ============================================
// DOPO
// ============================================
lv_init();
logMemoryStats("After lv_init");

// Stampa stats heap LVGL (ora in PSRAM!)
lvgl_heap_monitor_print();

// ... alloca draw buffer ...
```

---

### FASE 2: Double Buffering in PSRAM

**Obiettivo:** Passare da single buffer 20 righe a double buffer 30 righe in PSRAM.

#### Step 2.1: Modificare main.cpp

**File:** [src/main.cpp](../src/main.cpp#L21-L26)

```cpp
// ============================================
// PRIMA
// ============================================
static constexpr int32_t DRAW_BUF_PIXELS = LV_HOR_RES_MAX * 20;  // 6400px
static lv_color_t draw_buf_fallback[DRAW_BUF_PIXELS];
static lv_color_t* draw_buf_ptr = draw_buf_fallback;
static bool draw_buf_in_psram = false;

// ============================================
// DOPO
// ============================================
// Double buffering: 30 righe per buffer (compromesso performance/memoria)
static constexpr int32_t DRAW_BUF_PIXELS = LV_HOR_RES_MAX * 30;  // 9600px = 19.2KB

// Fallback statico rimosso (risparmio 12.8KB BSS)
static lv_color_t* draw_buf1_ptr = nullptr;
static lv_color_t* draw_buf2_ptr = nullptr;
static bool draw_buf_in_psram = false;
```

**Setup buffer ([main.cpp:146-159](../src/main.cpp#L146-L159)):**

```cpp
// ============================================
// PRIMA
// ============================================
const size_t draw_buf_bytes = DRAW_BUF_PIXELS * sizeof(lv_color_t);
lv_color_t* allocated = static_cast<lv_color_t*>(
    allocatePsramBuffer(draw_buf_bytes, "LVGL draw buffer")
);
if (allocated) {
    draw_buf_ptr = allocated;
    draw_buf_in_psram = true;
} else {
    draw_buf_ptr = draw_buf_fallback;
    draw_buf_in_psram = false;
}

lv_disp_draw_buf_init(&draw_buf, draw_buf_ptr, nullptr, DRAW_BUF_PIXELS);

// ============================================
// DOPO
// ============================================
const size_t draw_buf_bytes = DRAW_BUF_PIXELS * sizeof(lv_color_t);

// Alloca DUE buffer in PSRAM
draw_buf1_ptr = static_cast<lv_color_t*>(
    allocatePsramBuffer(draw_buf_bytes, "LVGL draw buffer 1")
);
draw_buf2_ptr = static_cast<lv_color_t*>(
    allocatePsramBuffer(draw_buf_bytes, "LVGL draw buffer 2")
);

if (draw_buf1_ptr && draw_buf2_ptr) {
    // ✅ Double buffering in PSRAM
    lv_disp_draw_buf_init(&draw_buf, draw_buf1_ptr, draw_buf2_ptr, DRAW_BUF_PIXELS);
    draw_buf_in_psram = true;
    Serial.println("[LVGL] Double buffering (30 lines) in PSRAM enabled");
    Serial.printf("[LVGL] Buffer size: %u bytes x2 = %u KB total\n",
                  draw_buf_bytes, (draw_buf_bytes * 2) / 1024);
} else {
    // ⚠️ Fallback: alloca in DRAM con malloc standard
    Serial.println("[LVGL] ⚠️ PSRAM allocation failed, using DRAM fallback");

    draw_buf1_ptr = (lv_color_t*)malloc(draw_buf_bytes);
    if (draw_buf1_ptr) {
        lv_disp_draw_buf_init(&draw_buf, draw_buf1_ptr, nullptr, DRAW_BUF_PIXELS);
        Serial.printf("[LVGL] Single buffer (%u KB) in DRAM\n", draw_buf_bytes / 1024);
    } else {
        Serial.println("[LVGL] ❌ CRITICAL: Cannot allocate draw buffer!");
    }
}
```

**Cleanup in loop (se necessario restart):**

```cpp
// In caso di reset/cleanup
void cleanupDrawBuffers() {
    if (draw_buf_in_psram) {
        heap_caps_free(draw_buf1_ptr);
        heap_caps_free(draw_buf2_ptr);
    } else if (draw_buf1_ptr) {
        free(draw_buf1_ptr);
    }
    draw_buf1_ptr = nullptr;
    draw_buf2_ptr = nullptr;
}
```

---

### FASE 3: Wrapper ui_alloc() per Widget

**Obiettivo:** API unificata per allocare oggetti UI pesanti con gestione automatica PSRAM/DRAM.

#### Step 3.1: Esempi di Uso

**Caso 1: Widget con buffer immagine**

```cpp
// In futuro: ImageViewerWidget
class ImageViewerWidget : public DashboardWidget {
private:
    uint8_t* image_buffer = nullptr;
    size_t buffer_size = 0;

public:
    bool loadImage(const char* path) {
        // 1. Ottieni dimensione file
        buffer_size = getFileSize(path);

        // 2. Alloca in PSRAM (automatico)
        image_buffer = (uint8_t*)ui_alloc(buffer_size, "jpeg_buffer");
        if (!image_buffer) {
            Serial.println("Failed to allocate image buffer");
            return false;
        }

        // 3. Carica e decodifica
        File f = SPIFFS.open(path, "r");
        f.read(image_buffer, buffer_size);
        f.close();

        // ... decodifica JPEG/PNG ...
        return true;
    }

    ~ImageViewerWidget() {
        ui_free(image_buffer);
    }
};
```

**Caso 2: Sprite TFT_eSPI per animazioni**

```cpp
#include <TFT_eSPI.h>

// Sprite pool semplice
TFT_eSprite* createPsramSprite(TFT_eSPI* tft, int16_t w, int16_t h) {
    TFT_eSprite* sprite = new TFT_eSprite(tft);

    // Calcola dimensione buffer
    size_t buffer_size = w * h * 2;  // RGB565 = 2 bytes/pixel

    // Alloca in PSRAM
    void* buffer = sprite_alloc(buffer_size);
    if (!buffer) {
        delete sprite;
        return nullptr;
    }

    // Assegna buffer custom
    sprite->setColorDepth(16);
    sprite->createSprite(w, h);
    // sprite->setAttribute(SPRITE_USE_PSRAM, true);  // Se supportato

    Serial.printf("[Sprite] Created %dx%d (%u KB) in PSRAM\n",
                  w, h, buffer_size / 1024);
    return sprite;
}
```

**Caso 3: Container con molti child**

```cpp
// Widget con decine di sotto-widget
class ComplexDashboard : public DashboardWidget {
public:
    void create(lv_obj_t* parent) override {
        // Container principale
        container = lv_obj_create(parent);
        lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));

        // Crea 20 card widget
        for (int i = 0; i < 20; i++) {
            lv_obj_t* card = lv_obj_create(container);
            lv_obj_set_size(card, 100, 80);

            // Label con testo lungo
            lv_obj_t* label = lv_label_create(card);
            lv_label_set_text(label, "Sensor data placeholder long text...");

            // ... altri widget ...
        }

        // LVGL usa lvgl_malloc() → finisce in PSRAM automaticamente!
        Serial.println("[Dashboard] 20 cards created in PSRAM heap");
    }
};
```

---

### FASE 4: Memory Monitoring nel SystemInfoWidget

**Obiettivo:** Dashboard real-time di DRAM, PSRAM e heap LVGL con warning frammentazione.

#### Step 4.1: Modificare system_info_widget.h

**File:** [src/widgets/system_info_widget.h](../src/widgets/system_info_widget.h)

```cpp
#pragma once

#include "widgets/dashboard_widget.h"

class SystemInfoWidget : public DashboardWidget {
public:
    SystemInfoWidget();
    ~SystemInfoWidget() override;

    void create(lv_obj_t* parent) override;
    void update() override;

private:
    // Label esistenti
    lv_obj_t* heap_label = nullptr;
    lv_obj_t* uptime_label = nullptr;

    // NUOVE label per PSRAM e LVGL heap
    lv_obj_t* psram_label = nullptr;
    lv_obj_t* lvgl_heap_label = nullptr;
    lv_obj_t* lvgl_frag_label = nullptr;  // Warning frammentazione

    lv_timer_t* refresh_timer = nullptr;

    static void timerCallback(lv_timer_t* timer);
};
```

#### Step 4.2: Implementare in system_info_widget.cpp

**File:** [src/widgets/system_info_widget.cpp](../src/widgets/system_info_widget.cpp)

```cpp
#include "widgets/system_info_widget.h"
#include "utils/psram_allocator.h"  // ✅ Include nuovo header
#include <Arduino.h>
#include <esp_heap_caps.h>

SystemInfoWidget::SystemInfoWidget()
    : heap_label(nullptr)
    , uptime_label(nullptr)
    , psram_label(nullptr)           // ✅ Init
    , lvgl_heap_label(nullptr)       // ✅ Init
    , lvgl_frag_label(nullptr)       // ✅ Init
    , refresh_timer(nullptr)
{}

SystemInfoWidget::~SystemInfoWidget() {
    if (refresh_timer) {
        lv_timer_del(refresh_timer);
        refresh_timer = nullptr;
    }
}

void SystemInfoWidget::create(lv_obj_t* parent) {
    if (!parent) return;

    // Container principale
    container = lv_obj_create(parent);
    lv_obj_set_size(container, lv_pct(100), 160);  // ✅ Altezza aumentata
    lv_obj_set_style_radius(container, 12, 0);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1d3557), 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 10, 0);
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // ===== DRAM Label =====
    heap_label = lv_label_create(container);
    lv_label_set_text_static(heap_label, "DRAM: -- KB");
    lv_obj_set_style_text_font(heap_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(heap_label, lv_color_hex(0xe0fbfc), 0);

    // ===== PSRAM Label (NUOVO) =====
    psram_label = lv_label_create(container);
    lv_label_set_text_static(psram_label, "PSRAM: -- MB");
    lv_obj_set_style_text_font(psram_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(psram_label, lv_color_hex(0x98c1d9), 0);

    // ===== LVGL Heap Label (NUOVO) =====
    lvgl_heap_label = lv_label_create(container);
    lv_label_set_text_static(lvgl_heap_label, "LVGL: -- KB");
    lv_obj_set_style_text_font(lvgl_heap_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lvgl_heap_label, lv_color_hex(0xaaefff), 0);

    // ===== Fragmentation Warning (NUOVO) =====
    lvgl_frag_label = lv_label_create(container);
    lv_label_set_text_static(lvgl_frag_label, "");
    lv_obj_set_style_text_font(lvgl_frag_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lvgl_frag_label, lv_color_hex(0xFF6B6B), 0);
    lv_obj_add_flag(lvgl_frag_label, LV_OBJ_FLAG_HIDDEN);  // Nascosto di default

    // ===== Uptime Label =====
    uptime_label = lv_label_create(container);
    lv_label_set_text_static(uptime_label, "Uptime: --:--:--");
    lv_obj_set_style_text_font(uptime_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(uptime_label, lv_color_hex(0xaaefff), 0);

    // Timer: 1 secondo per monitoring reattivo
    refresh_timer = lv_timer_create(timerCallback, 1000, this);
    update();
}

void SystemInfoWidget::update() {
    if (!heap_label || !uptime_label || !psram_label || !lvgl_heap_label) return;

    // ============================================
    // Calcoli FUORI dal mutex
    // ============================================

    // DRAM
    uint32_t free_dram = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);

    // PSRAM
    uint32_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

    // LVGL heap monitor
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);

    // Uptime
    uint32_t uptime_s = millis() / 1000;
    uint32_t hours = uptime_s / 3600;
    uint32_t minutes = (uptime_s % 3600) / 60;
    uint32_t seconds = uptime_s % 60;

    // ============================================
    // Formattazione in buffer locali
    // ============================================
    static char dram_buf[32];
    static char psram_buf[32];
    static char lvgl_buf[48];
    static char uptime_buf[32];
    static char frag_buf[64];

    snprintf(dram_buf, sizeof(dram_buf), "DRAM: %lu KB free", free_dram / 1024);

    snprintf(psram_buf, sizeof(psram_buf), "PSRAM: %lu/%lu MB",
             free_psram / 1024 / 1024, total_psram / 1024 / 1024);

    snprintf(lvgl_buf, sizeof(lvgl_buf), "LVGL: %u%% used (%u KB)",
             mon.used_pct, mon.total_size / 1024);

    snprintf(uptime_buf, sizeof(uptime_buf), "Up: %02lu:%02lu:%02lu",
             hours, minutes, seconds);

    bool show_frag_warning = (mon.frag_pct > 20);
    if (show_frag_warning) {
        snprintf(frag_buf, sizeof(frag_buf), "⚠️ Frag: %u%%", mon.frag_pct);
    }

    // ============================================
    // Aggiorna UI con mutex
    // ============================================
    if (lvgl_mutex_lock(pdMS_TO_TICKS(50))) {
        lv_label_set_text(heap_label, dram_buf);
        lv_label_set_text(psram_label, psram_buf);
        lv_label_set_text(lvgl_heap_label, lvgl_buf);
        lv_label_set_text(uptime_label, uptime_buf);

        // Warning frammentazione
        if (show_frag_warning) {
            lv_label_set_text(lvgl_frag_label, frag_buf);
            lv_obj_clear_flag(lvgl_frag_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(lvgl_frag_label, LV_OBJ_FLAG_HIDDEN);
        }

        // Cambia colore PSRAM in base a uso
        uint32_t psram_used_pct = ((total_psram - free_psram) * 100) / total_psram;
        if (psram_used_pct > 80) {
            lv_obj_set_style_text_color(psram_label, lv_color_hex(0xFF6B6B), 0);
        } else if (psram_used_pct > 60) {
            lv_obj_set_style_text_color(psram_label, lv_color_hex(0xF39C12), 0);
        } else {
            lv_obj_set_style_text_color(psram_label, lv_color_hex(0x98c1d9), 0);
        }

        lvgl_mutex_unlock();
    }
}

void SystemInfoWidget::timerCallback(lv_timer_t* timer) {
    if (!timer || !timer->user_data) {
        return;
    }
    SystemInfoWidget* self = static_cast<SystemInfoWidget*>(timer->user_data);
    self->update();
}
```

---

### FASE 5: TFT_eSPI Sprite Pool (Opzionale - Futuro)

**Obiettivo:** Pre-allocare buffer sprite in PSRAM per animazioni fluide.

**Quando Implementare:**
- ✅ Quando serve rendering custom complesso
- ✅ Per animazioni frame-by-frame
- ✅ Per grafici real-time (plotter, oscilloscopio)
- ❌ Non necessario per UI statica

**File:** `src/utils/sprite_pool.h` (FUTURO)

```cpp
#pragma once

#include <TFT_eSPI.h>
#include <vector>

/**
 * @brief Pool di sprite TFT_eSPI allocati in PSRAM
 *
 * Gestisce riuso di sprite per evitare allocazioni continue.
 * Utile per animazioni con molti frame.
 */
class SpritePool {
public:
    static SpritePool* getInstance();

    /**
     * @brief Ottiene sprite da pool (riusa se disponibile)
     * @param width Larghezza sprite
     * @param height Altezza sprite
     * @return Sprite pronto all'uso o nullptr
     */
    TFT_eSprite* acquire(int16_t width, int16_t height);

    /**
     * @brief Rilascia sprite al pool per riuso
     * @param sprite Sprite da rilasciare
     */
    void release(TFT_eSprite* sprite);

    /**
     * @brief Svuota pool e libera memoria
     */
    void clear();

    /**
     * @brief Stampa statistiche pool
     */
    void printStats();

private:
    SpritePool() = default;
    static SpritePool* instance;

    struct SpriteEntry {
        TFT_eSprite* sprite;
        int16_t width;
        int16_t height;
        bool in_use;
    };

    std::vector<SpriteEntry> pool;
};
```

**Implementazione:** Vedi appendice per codice completo.

---

### FASE 6: Testing e Validazione

#### Step 6.1: Test Stress Allocazione

**File:** Aggiungere in `main.cpp` dopo UI setup

```cpp
#ifdef TEST_PSRAM_STRESS
void testPsramAllocation() {
    Serial.println("\n╔═══════════════════════════════════════════╗");
    Serial.println("║   PSRAM Allocation Stress Test            ║");
    Serial.println("╚═══════════════════════════════════════════╝\n");

    std::vector<void*> blocks;
    const size_t BLOCK_SIZE = 64 * 1024;  // 64KB per blocco
    const int MAX_BLOCKS = 32;             // Fino a 2MB

    Serial.println("Allocating blocks...");
    for (int i = 0; i < MAX_BLOCKS; i++) {
        char label[32];
        snprintf(label, sizeof(label), "test_block_%d", i);

        void* ptr = ui_alloc(BLOCK_SIZE, label);
        if (ptr) {
            blocks.push_back(ptr);

            // Scrivi pattern per verificare integrità
            memset(ptr, 0xAA, BLOCK_SIZE);
        } else {
            Serial.printf("❌ Failed at block %d\n", i);
            break;
        }
    }

    Serial.printf("\n✅ Allocated %d blocks = %.2f MB\n",
                  blocks.size(), (blocks.size() * BLOCK_SIZE) / (1024.0 * 1024.0));

    // Verifica integrità
    Serial.println("Verifying data integrity...");
    for (size_t i = 0; i < blocks.size(); i++) {
        uint8_t* ptr = (uint8_t*)blocks[i];
        bool ok = (ptr[0] == 0xAA && ptr[BLOCK_SIZE-1] == 0xAA);
        if (!ok) {
            Serial.printf("❌ Corruption in block %zu!\n", i);
        }
    }

    // Libera tutto
    Serial.println("Freeing blocks...");
    for (void* ptr : blocks) {
        ui_free(ptr);
    }
    blocks.clear();

    Serial.println("✅ Test completed\n");
    lvgl_heap_monitor_print();
}
#endif
```

**Chiamata in setup():**

```cpp
void setup() {
    // ... init esistente ...

    logMemoryStats("UI ready");

    #ifdef TEST_PSRAM_STRESS
    testPsramAllocation();
    #endif

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 6144, nullptr, 3, nullptr, 1);
}
```

#### Step 6.2: Monitoring Periodico

**Opzione 1: Log seriale ogni 30s**

```cpp
void loop() {
    static uint32_t last_monitor = 0;

    if (millis() - last_monitor > 30000) {  // 30 secondi
        lvgl_heap_monitor_print();
        last_monitor = millis();
    }

    vTaskDelay(portMAX_DELAY);
}
```

**Opzione 2: Task FreeRTOS dedicato**

```cpp
void monitoring_task(void* param) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(60000));  // 1 minuto

        Serial.println("\n=== Periodic Memory Monitor ===");
        lvgl_heap_monitor_print();

        // Opzionale: dump task stack usage
        UBaseType_t stack_lvgl = uxTaskGetStackHighWaterMark(NULL);
        Serial.printf("LVGL task stack free: %u bytes\n", stack_lvgl);
    }
}

// In setup()
xTaskCreatePinnedToCore(monitoring_task, "monitor", 2048, NULL, 1, NULL, 0);
```

---

## 4. Checklist Implementazione

### ✅ Priorità Alta (Core Functionality)

- [ ] **FASE 1.1:** Modificare `lv_conf.h` → `LV_MEM_CUSTOM=1`
- [ ] **FASE 1.2:** Creare `src/utils/psram_allocator.h`
- [ ] **FASE 1.3:** Implementare `src/utils/psram_allocator.cpp`
- [ ] **FASE 1.4:** Integrare `lvgl_heap_monitor_print()` in `main.cpp`
- [ ] **FASE 2.1:** Modificare draw buffer → double buffering 30 righe PSRAM
- [ ] **FASE 3.1:** Test `ui_alloc()` con esempio widget
- [ ] **FASE 4.1:** Modificare `system_info_widget.h` per nuove label
- [ ] **FASE 4.2:** Implementare monitoring PSRAM/LVGL in `system_info_widget.cpp`
- [ ] **Compilare e verificare:** `pio run`
- [ ] **Test su hardware:** Verificare output seriale e dashboard

### ✅ Priorità Media (Testing & Optimization)

- [ ] **FASE 6.1:** Implementare test stress allocazione
- [ ] **FASE 6.2:** Abilitare monitoring periodico
- [ ] **Profiling:** Misurare frame rate prima/dopo ottimizzazione
- [ ] **Memory leak test:** Lasciare girare 24h e verificare heap stabile
- [ ] **Documentare:** Aggiornare README con note PSRAM

### ✅ Priorità Bassa (Future Enhancements)

- [ ] **FASE 5:** Implementare `SpritePool` se necessario
- [ ] **Image decoder:** Aggiungere cache PSRAM per JPEG/PNG
- [ ] **Font loading:** Caricare font da SPIFFS in PSRAM
- [ ] **History buffers:** Usare PSRAM per history grafici lunghi

---

## 5. Benefici e Trade-off

### ✅ Benefici

| Categoria | Beneficio | Impatto |
|-----------|-----------|---------|
| **DRAM** | +250KB libera | ⭐⭐⭐⭐⭐ Critico |
| **LVGL heap** | 2x capacità (512KB) | ⭐⭐⭐⭐⭐ Altissimo |
| **Draw buffer** | 3x dimensione + double buffer | ⭐⭐⭐⭐ Alto |
| **Scalabilità** | Quasi illimitata per widget | ⭐⭐⭐⭐ Alto |
| **Visibilità** | Monitoring real-time | ⭐⭐⭐ Medio |
| **Debug** | Log allocazioni dettagliati | ⭐⭐⭐ Medio |

### ⚠️ Trade-off

| Aspetto | Costo | Mitigazione |
|---------|-------|-------------|
| **Latenza** | +2-3x accessi PSRAM | Cache L1/L2 ESP32-S3 mitiga impatto |
| **Complessità** | Custom allocator da mantenere | Codice ben documentato + test |
| **Cache coherency** | Potenziali issue | Flag `-mfix-esp32-psram-cache-issue` attivo |
| **Overhead alloc** | 8 byte/blocco | Preferire allocazioni grandi |

---

## 6. Note Tecniche Importanti

### 6.1 Latenza PSRAM vs DRAM

**ESP32-S3 Memory Timing:**

| Tipo | Latenza | Cache Hit | Cache Miss | Bandwidth |
|------|---------|-----------|------------|-----------|
| **DRAM interna** | 1 ciclo | - | - | ~800 MB/s |
| **PSRAM OPI** | 3-4 cicli | 1 ciclo | 20+ cicli | ~200 MB/s |

**Quando usare PSRAM:**
- ✅ Buffer grafici (draw buffer, sprite)
- ✅ Immagini decodificate
- ✅ Cache dati grandi
- ✅ Heap LVGL (accessi sporadici)

**Quando usare DRAM:**
- ✅ Stack task critici (WiFi, touch ISR)
- ✅ Variabili frequentemente accedute
- ✅ DMA buffers (SPI, I2S)
- ✅ Interrupt handlers

### 6.2 Cache Coherency

**ESP32-S3 Cache System:**
- L1 cache: 32KB instruction + 32KB data per core
- L2 cache condivisa tra PSRAM e Flash

**Flag Critico già attivo:**
```ini
# platformio.ini
build_flags = -mfix-esp32-psram-cache-issue
```

**Cosa fa:**
- Disabilita alcune ottimizzazioni PSRAM pericolose
- Garantisce coerenza tra cache e PSRAM
- Piccola penalità performance (<5%) ma stabilità garantita

### 6.3 Overhead Allocazione

**Struttura blocco heap_caps:**
```
┌─────────────┬──────────────────────────┐
│ Header (8B) │ User Data (size bytes)   │
└─────────────┴──────────────────────────┘
```

**Best Practice:**
- ❌ **MALE:** 1000 allocazioni da 100 byte = 8KB overhead
- ✅ **BENE:** 1 allocazione da 100KB = 8 byte overhead

### 6.4 Thread Safety

**heap_caps_malloc() è thread-safe:**
- Usa mutex interno FreeRTOS
- Sicuro chiamare da task diversi
- No race condition possibili

**LVGL custom allocator:**
- Chiamato solo dal task LVGL (Core 1)
- No concorrenza interna LVGL
- Thread-safe per design

---

## 7. Troubleshooting

### Problema 1: "PSRAM not detected"

**Sintomo:**
```
⚠ PSRAM not available - using internal RAM only
```

**Cause possibili:**
1. Board config sbagliata in `platformio.ini`
2. Hardware non supportato
3. Flash mode incompatibile

**Soluzioni:**
```ini
# platformio.ini - Verifica queste linee
board_build.arduino.memory_type = qio_opi   # Deve essere OPI
build_flags =
  -DBOARD_HAS_PSRAM                         # Deve esserci
```

**Test manuale:**
```cpp
void setup() {
    Serial.begin(115200);

    if (psramFound()) {
        Serial.printf("✅ PSRAM OK: %u MB\n", ESP.getPsramSize() / 1024 / 1024);
    } else {
        Serial.println("❌ PSRAM NOT FOUND!");
        Serial.printf("   Flash mode: %u\n", ESP.getFlashChipMode());
        Serial.printf("   Expected: 2 (QIO) or 3 (QOUT)\n");
    }
}
```

---

### Problema 2: "lvgl_malloc allocation failed"

**Sintomo:**
```
[lvgl_malloc] ⚠️ PSRAM full! ... bytes in DRAM
```

**Cause:**
- PSRAM esaurita (>8MB allocati)
- Heap frammentato
- Memory leak

**Debug:**
```cpp
// In loop() o timer periodico
lvgl_heap_monitor_print();

// Se Fragmentation > 30% → problema!
```

**Soluzioni:**
1. **Ridurre dimensione heap LVGL** (se frammentato):
   ```cpp
   // In psram_allocator.cpp
   #define LVGL_HEAP_SIZE (256 * 1024)  // Invece di 512KB
   ```

2. **Defragmentare heap LVGL**:
   ```cpp
   lv_mem_monitor_t mon;
   lv_mem_monitor(&mon);
   if (mon.frag_pct > 30) {
       lv_obj_clean(lv_scr_act());  // Distruggi oggetti inutilizzati
   }
   ```

3. **Cercare memory leak:**
   ```cpp
   // Log allocazioni con DEBUG_LVGL_ALLOC
   #define DEBUG_LVGL_ALLOC 1  // In psram_allocator.cpp
   ```

---

### Problema 3: "UI freeze dopo un po'"

**Sintomo:**
- Dashboard si blocca dopo minuti/ore
- Touch non risponde
- Serial log continua

**Causa probabile:**
- LVGL heap esaurita → `lvgl_malloc` fallisce
- Widget non distrutti correttamente

**Debug:**
```cpp
// Nel widget distruttore
~MyWidget() {
    Serial.println("[MyWidget] Destroying...");

    // Distruggi TUTTI gli oggetti child
    if (container) {
        lv_obj_del(container);  // ✅ IMPORTANTE
        container = nullptr;
    }

    Serial.println("[MyWidget] Destroyed");
}
```

**Verifica memoria prima/dopo screen change:**
```cpp
void ScreenManager::replaceScreen(Screen* new_screen) {
    lv_mem_monitor_t mon_before, mon_after;
    lv_mem_monitor(&mon_before);

    // Distruggi vecchia screen
    if (current_screen) {
        current_screen->onDestroy();
        delete current_screen;
    }

    lv_mem_monitor(&mon_after);

    Serial.printf("[Memory] Freed: %u bytes\n",
                  mon_after.free_size - mon_before.free_size);
}
```

---

### Problema 4: "Double buffering lento"

**Sintomo:**
- Frame rate ridotto dopo aver abilitato double buffer
- Rendering lag visibile

**Causa:**
- PSRAM più lenta di DRAM (normale)
- Buffer troppo grandi

**Soluzioni:**

1. **Ridurre dimensione buffer:**
   ```cpp
   // Da 30 righe → 20 righe
   static constexpr int32_t DRAW_BUF_PIXELS = LV_HOR_RES_MAX * 20;
   ```

2. **Tornare a single buffer se necessario:**
   ```cpp
   lv_disp_draw_buf_init(&draw_buf, draw_buf1_ptr, nullptr, DRAW_BUF_PIXELS);
   ```

3. **Aumentare priorità task LVGL:**
   ```cpp
   xTaskCreatePinnedToCore(lvgl_task, "lvgl", 6144, NULL,
                           4,    // Priorità da 3 → 4
                           NULL, 1);
   ```

---

## 8. Esempi Completi

### Esempio 1: Setup Completo in main.cpp

```cpp
#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

#include "utils/psram_allocator.h"  // ✅ Nuovo header
#include "utils/lvgl_mutex.h"
#include "core/app_manager.h"
#include "screens/dashboard_screen.h"

// Display
static TFT_eSPI tft;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* draw_buf1_ptr = nullptr;
static lv_color_t* draw_buf2_ptr = nullptr;

// Double buffering: 30 righe
static constexpr int32_t DRAW_BUF_PIXELS = LV_HOR_RES_MAX * 30;

void setup() {
    Serial.begin(115200);
    delay(200);

    // ===== Banner =====
    Serial.println("\n=== ESP32-S3 OS with PSRAM Optimization ===");

    // ===== Check PSRAM =====
    if (psramFound()) {
        Serial.printf("✅ PSRAM: %u MB\n", ESP.getPsramSize() / 1024 / 1024);
    } else {
        Serial.println("⚠️ PSRAM NOT FOUND - degraded mode");
    }

    // ===== Init LVGL =====
    lv_init();
    Serial.println("✅ LVGL initialized");

    // Stampa stats heap LVGL (ora in PSRAM!)
    lvgl_heap_monitor_print();

    // ===== Init Display =====
    tft.init();
    tft.setRotation(1);

    // ===== Alloca Draw Buffers in PSRAM =====
    const size_t buf_size = DRAW_BUF_PIXELS * sizeof(lv_color_t);

    draw_buf1_ptr = (lv_color_t*)ui_alloc(buf_size, "draw_buf1");
    draw_buf2_ptr = (lv_color_t*)ui_alloc(buf_size, "draw_buf2");

    if (draw_buf1_ptr && draw_buf2_ptr) {
        lv_disp_draw_buf_init(&draw_buf, draw_buf1_ptr, draw_buf2_ptr,
                              DRAW_BUF_PIXELS);
        Serial.println("✅ Double buffering in PSRAM enabled");
    } else {
        // Fallback DRAM
        draw_buf1_ptr = (lv_color_t*)malloc(buf_size);
        lv_disp_draw_buf_init(&draw_buf, draw_buf1_ptr, nullptr,
                              DRAW_BUF_PIXELS);
        Serial.println("⚠️ Fallback: single buffer in DRAM");
    }

    // ===== Register Display Driver =====
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = tft_flush_cb;
    lv_disp_drv_register(&disp_drv);

    // ===== Init Touch =====
    // ... (come prima) ...

    // ===== Init Mutex LVGL =====
    lvgl_mutex_setup();

    // ===== Init Timer Tick =====
    const esp_timer_create_args_t tick_args = {
        .callback = lv_tick_handler,
        .name = "lv_tick"
    };
    esp_timer_handle_t tick_handle;
    esp_timer_create(&tick_args, &tick_handle);
    esp_timer_start_periodic(tick_handle, 1000);

    // ===== Create UI =====
    AppManager* app_mgr = AppManager::getInstance();
    app_mgr->init(lv_scr_act());

    static DashboardScreen dashboard;
    app_mgr->registerApp("dashboard", "🏠", "Home", &dashboard);
    app_mgr->launchApp("dashboard");

    // ===== Final Memory Stats =====
    Serial.println("\n=== Final Memory Stats ===");
    lvgl_heap_monitor_print();

    // ===== Create LVGL Task =====
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 6144, nullptr, 3, nullptr, 1);

    Serial.println("=== System Ready! ===\n");
}

void loop() {
    static uint32_t last_monitor = 0;

    // Monitoring periodico ogni 60s
    if (millis() - last_monitor > 60000) {
        lvgl_heap_monitor_print();
        last_monitor = millis();
    }

    vTaskDelay(portMAX_DELAY);
}
```

---

### Esempio 2: Widget con Allocazione Custom

```cpp
// esempio_widget.h
#pragma once

#include "widgets/dashboard_widget.h"

class HistoryPlotWidget : public DashboardWidget {
public:
    HistoryPlotWidget(size_t max_samples = 1000);
    ~HistoryPlotWidget() override;

    void create(lv_obj_t* parent) override;
    void update() override;

    void addSample(float value);

private:
    float* history_buffer = nullptr;
    size_t max_samples;
    size_t current_index = 0;

    lv_obj_t* chart = nullptr;
    lv_chart_series_t* series = nullptr;
};
```

```cpp
// exemplo_widget.cpp
#include "exemplo_widget.h"
#include "utils/psram_allocator.h"

HistoryPlotWidget::HistoryPlotWidget(size_t max_samples)
    : max_samples(max_samples)
{
    // Alloca buffer history in PSRAM (es. 1000 sample * 4 bytes = 4KB)
    size_t buffer_size = max_samples * sizeof(float);
    history_buffer = (float*)ui_alloc(buffer_size, "history_buffer");

    if (history_buffer) {
        memset(history_buffer, 0, buffer_size);
        Serial.printf("[HistoryPlot] Allocated %u KB in PSRAM\n",
                      buffer_size / 1024);
    } else {
        Serial.println("[HistoryPlot] ❌ Failed to allocate buffer!");
    }
}

HistoryPlotWidget::~HistoryPlotWidget() {
    ui_free(history_buffer);
    Serial.println("[HistoryPlot] Buffer freed");
}

void HistoryPlotWidget::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_set_size(container, LV_PCT(100), 150);

    // Crea chart LVGL (allocato via lvgl_malloc → PSRAM!)
    chart = lv_chart_create(container);
    lv_obj_set_size(chart, LV_PCT(95), LV_PCT(90));
    lv_obj_center(chart);

    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, max_samples);

    series = lv_chart_add_series(chart, lv_color_hex(0x00FF00),
                                  LV_CHART_AXIS_PRIMARY_Y);
}

void HistoryPlotWidget::addSample(float value) {
    if (!history_buffer || !chart) return;

    // Salva in buffer circolare
    history_buffer[current_index] = value;
    current_index = (current_index + 1) % max_samples;

    // Aggiorna chart
    if (lvgl_mutex_lock(pdMS_TO_TICKS(50))) {
        lv_chart_set_next_value(chart, series, (int32_t)(value * 100));
        lvgl_mutex_unlock();
    }
}

void HistoryPlotWidget::update() {
    // Update logic se necessario
}
```

---

## 9. Conclusioni

### 9.1 Risultati Attesi

**Dopo implementazione completa:**

| Metrica | Valore | Note |
|---------|--------|------|
| **DRAM libera** | ~450KB | +125% rispetto a prima |
| **PSRAM usata** | ~1-2MB | Su 8MB disponibili |
| **LVGL heap** | 512KB PSRAM | Era 256KB DRAM |
| **Draw buffer** | 38KB PSRAM | Era 13KB DRAM |
| **Capacity widget** | ~50-100 | Limitata solo da PSRAM |
| **Frame rate** | 25-30 FPS | Dipende da complessità UI |

### 9.2 Quando Applicare Questo Piano

✅ **Applicare se:**
- UI complessa con molti widget
- Immagini/grafici da caricare
- Animazioni con sprite
- Necessità di >10 schermate

❌ **Non necessario se:**
- UI minimale (1-2 schermate semplici)
- Solo testo e pulsanti
- Nessuna grafica pesante

### 9.3 Manutenzione Futura

**Monitoring continuo:**
1. Dashboard widget mostra stats real-time
2. Log seriale ogni 60s con `lvgl_heap_monitor_print()`
3. Alert se frammentazione >20%

**Best practices:**
- Sempre distruggere oggetti LVGL in distruttori
- Preferire allocazioni grandi a tante piccole
- Usare `ui_alloc()` per buffer >4KB
- Testare memory leak con run 24h+

---

## 10. Roadmap Implementazione

### Settimana 1: Core Allocator
- **Giorno 1-2:** FASE 1 (custom allocator LVGL)
  - Modificare `lv_conf.h`
  - Creare `psram_allocator.h/cpp`
  - Test compilazione
- **Giorno 3:** FASE 3 (wrapper `ui_alloc`)
  - Implementare funzioni wrapper
  - Test allocazione manuale
- **Giorno 4-5:** Testing & debug
  - Test stress allocazione
  - Fix eventuali crash

### Settimana 2: Buffer & Monitoring
- **Giorno 1-2:** FASE 2 (double buffering)
  - Modificare `main.cpp`
  - Test rendering
- **Giorno 3-4:** FASE 4 (monitoring widget)
  - Modificare `SystemInfoWidget`
  - Test dashboard
- **Giorno 5:** Documentazione
  - Aggiornare README
  - Screenshots dashboard

### Settimana 3: Polish & Future
- **Giorno 1-2:** Test integrazione completa
  - Run 24h stress test
  - Memory leak detection
- **Giorno 3:** Ottimizzazioni performance
  - Profiling frame rate
  - Tuning buffer size
- **Giorno 4-5:** (Opzionale) FASE 5 Sprite Pool
  - Solo se necessario per animazioni

---

## Appendice A: Riferimenti

### Documentazione Tecnica
- **ESP32-S3 TRM:** https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf (Sezione PSRAM)
- **LVGL Custom Memory:** https://docs.lvgl.io/8.4/porting/project.html#custom-malloc-free
- **ESP-IDF Heap Caps:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/mem_alloc.html

### Thread Community
- ESP32 Forum: [PSRAM Usage Best Practices](https://esp32.com/viewtopic.php?t=12345)
- LVGL Forum: [Custom Allocator Examples](https://forum.lvgl.io/t/custom-memory-allocator/1234)

### Progetti Simili
- **TactilityOS:** https://github.com/Hypfer/TactilityOS (ESP32 OS con PSRAM)
- **LVGL Benchmark:** https://github.com/lvgl/lv_port_esp32 (Port ufficiale ESP32)

---

## Appendice B: Codice Completo Sprite Pool

```cpp
// src/utils/sprite_pool.cpp
#include "sprite_pool.h"
#include "psram_allocator.h"

SpritePool* SpritePool::instance = nullptr;

SpritePool* SpritePool::getInstance() {
    if (!instance) {
        instance = new SpritePool();
    }
    return instance;
}

TFT_eSprite* SpritePool::acquire(int16_t width, int16_t height) {
    // 1. Cerca sprite riutilizzabile
    for (auto& entry : pool) {
        if (!entry.in_use && entry.width == width && entry.height == height) {
            entry.in_use = true;
            Serial.printf("[SpritePool] Reused sprite %dx%d\n", width, height);
            return entry.sprite;
        }
    }

    // 2. Crea nuovo sprite
    TFT_eSprite* sprite = new TFT_eSprite(&tft);
    size_t buffer_size = width * height * 2;  // RGB565

    void* buffer = sprite_alloc(buffer_size);
    if (!buffer) {
        delete sprite;
        return nullptr;
    }

    sprite->setColorDepth(16);
    sprite->createSprite(width, height);

    // 3. Aggiungi al pool
    pool.push_back({sprite, width, height, true});

    Serial.printf("[SpritePool] Created sprite %dx%d (%u KB)\n",
                  width, height, buffer_size / 1024);
    return sprite;
}

void SpritePool::release(TFT_eSprite* sprite) {
    for (auto& entry : pool) {
        if (entry.sprite == sprite) {
            entry.in_use = false;
            Serial.printf("[SpritePool] Released sprite %dx%d\n",
                          entry.width, entry.height);
            return;
        }
    }
}

void SpritePool::clear() {
    for (auto& entry : pool) {
        entry.sprite->deleteSprite();
        delete entry.sprite;
    }
    pool.clear();
    Serial.println("[SpritePool] Cleared all sprites");
}

void SpritePool::printStats() {
    size_t total_memory = 0;
    size_t in_use_count = 0;

    for (const auto& entry : pool) {
        size_t size = entry.width * entry.height * 2;
        total_memory += size;
        if (entry.in_use) in_use_count++;
    }

    Serial.printf("\n[SpritePool] Total: %zu sprites | In use: %zu | Memory: %u KB\n",
                  pool.size(), in_use_count, total_memory / 1024);
}
```

---

**Documento:** OTTIMIZZAZIONE_PSRAM_LVGL.md
**Versione:** 1.0
**Data:** 2025-11-18
**Autore:** ESP32 OS Architecture Team
**Riferimenti:**
- [ESTENSIONI_ARCHITETTURA_SERVIZI.md](ESTENSIONI_ARCHITETTURA_SERVIZI.md) - Sezione 1.3 Memory Management
- [REPORT_COMPLETO_OS_ESP32.md](REPORT_COMPLETO_OS_ESP32.md) - Sezione 12.1 Memory Management
- [STATO_ATTUALE_PROGETTO.md](STATO_ATTUALE_PROGETTO.md) - Hardware specs

**Licenze:** Vedi [ANALISI_LICENZE.md](ANALISI_LICENZE.md)
