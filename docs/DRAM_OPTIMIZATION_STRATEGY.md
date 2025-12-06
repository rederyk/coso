# Strategia di Ottimizzazione DRAM

## Analisi della situazione attuale

### Memoria disponibile
- **DRAM heap libera**: ~20 KB (di ~320 KB totali)
- **PSRAM libera**: ~7.9 MB (di ~8 MB totali)

### Configurazione buffer display
- **Modalità**: `LVGL_BUFFER_MODE=0` (PSRAM Single Buffer)
- **Dimensione**: 320×24 pixels × 2 bytes = **15 KB in PSRAM**
- **Conclusione**: Il buffer display **NON occupa DRAM**

## Dove va la DRAM?

### Allocazioni fisse (~250-270 KB)
1. **Task stacks** (~50-60 KB totali):
   - LVGL task: ~8-12 KB
   - Network/WiFi task: ~8-12 KB
   - Audio task: ~4-8 KB
   - Altri task (BLE, logging, etc): ~20-30 KB

2. **Librerie e driver** (~100-150 KB):
   - TFT_eSPI driver buffers
   - WiFi/BLE stack internal buffers
   - FreeRTOS kernel structures
   - Interrupt vectors e BSS section

3. **Heap allocato dinamicamente** (~50-70 KB):
   - Code FreeRTOS (message queues)
   - Buffer temporanei per network
   - Logger ring buffer
   - Strutture dati delle app

## Cosa fa attualmente il suspend LVGL

### Durante suspend (implementazione corrente)
✅ Pausa timer LVGL → risparmia CPU, non memoria
✅ Pulisce cache temporanee (`lv_mem_buf_free_all()`) → libera ~2-5 KB DRAM
✅ Spegne backlight → risparmia energia
❌ **NON** distrugge oggetti UI → rimangono in PSRAM (corretto!)

### Memoria liberata
- **PSRAM**: 0 KB (gli oggetti UI non vengono distrutti)
- **DRAM**: ~2-5 KB (solo cache temporanee)
- **Energia**: Significativa (schermo spento + CPU idle)

## Perché NON spostare buffer DRAM ↔ PSRAM

### 1. Il buffer display è già in PSRAM
Con `LVGL_BUFFER_MODE=0`, il draw buffer è allocato in PSRAM, non in DRAM.

### 2. Non ci sono grandi buffer da spostare
I 20 KB liberi di DRAM sono frammentati tra:
- Stack (non spostabili)
- Driver buffers (necessari per DMA e periferiche)
- Heap fragmentation

### 3. Complessità vs beneficio
Spostare buffer avanti e indietro richiederebbe:
- `memcpy()` di grandi quantità di dati (lento)
- Gestione complessa di puntatori
- Rischio di corruzione memoria
- Guadagno: ~5-10 KB DRAM al massimo

## Strategia alternativa: Ottimizzazione mirata

### Opzione 1: Ridurre gli stack dei task (moderato impatto)
```cpp
// In main.cpp o task creation
xTaskCreatePinnedToCore(
    lvgl_task,
    "LVGL",
    6144,  // Ridotto da 8192 (risparmio: 2 KB)
    ...
);
```
**Rischio**: Stack overflow se il task usa più memoria del previsto.

### Opzione 2: Usare PSRAM per buffer audio/network (alto impatto)
```cpp
// Invece di malloc()
uint8_t* audio_buf = (uint8_t*)heap_caps_malloc(
    AUDIO_BUF_SIZE,
    MALLOC_CAP_SPIRAM  // Forza PSRAM
);
```
**Beneficio**: Libera 10-30 KB DRAM
**Rischio**: Performance degradation se usato con DMA

### Opzione 3: Modalità voice-only con LVGL completamente deinit
Invece di suspend leggero, implementare un vero deinit/reinit:
```cpp
// Durante voice mode prolungato
lv_deinit();  // Libera TUTTO
// ... usa voice assistant ...
lv_init();    // Reinizializza da zero
```
**Beneficio**: Libera ~40-60 KB DRAM
**Svantaggio**: Resume lento (~2-3 secondi)

## Raccomandazione finale

### Per uso normale
✅ **Mantieni la strategia attuale (suspend leggero)**:
- Nessun crash
- Resume istantaneo
- Risparmio energetico eccellente
- PSRAM abbondante per oggetti UI

### Se serve DAVVERO più DRAM
1. **Identifica i colpevoli** usando il nuovo pulsante "Analyze DRAM Usage"
2. **Sposta buffer audio/network in PSRAM** (caso per caso)
3. **Riduci stack size** solo se sicuro
4. **Ultima spiaggia**: Implementa deinit completo per voice-only mode

### Uso del nuovo tool diagnostico
```cpp
// In developer screen, premere il pulsante:
"Analyze DRAM Usage"

// Output:
// - Heap totale/libero/usato
// - Largest free block (frammentazione)
// - Stima stack usage
// - Breakdown di dove va la memoria
```

## Conclusione

**La PSRAM è il tuo "swap"**. Con 7.9 MB disponibili, puoi:
- Mantenere oggetti UI sempre in memoria
- Allocare grandi buffer temporanei
- Evitare swap DRAM↔PSRAM che è lento e fragile

**La DRAM è preziosa ma limitata**. Usa analisi mirata per trovare sprechi, non spostamenti generici di buffer.
