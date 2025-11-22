# Guida: Testing Buffer LVGL

**Riferimento:** [lvgl_buffer_analysis.md](lvgl_buffer_analysis.md)

## Configurazione Compile-Time

Il progetto supporta ora **3 modalità di buffer LVGL** selezionabili tramite define `LVGL_BUFFER_MODE`:

| Modalità | Nome | Descrizione | RAM Usata | Performance DMA |
|----------|------|-------------|-----------|-----------------|
| **0** | PSRAM Single | Buffer in PSRAM (baseline) | 0 KB DRAM | ⚠️ Media (40-60 MB/s) |
| **1** | DRAM Single | Buffer in RAM interna **(default)** | 15 KB DRAM | ✅ Alta (80 MB/s) |
| **2** | DRAM Double | Double buffer in RAM | 30 KB DRAM | ✅ Ottima (80 MB/s + zero contese) |

**Default:** Modalità **1** (DRAM Single) - miglior bilanciamento performance/memoria

---

## Come Testare Diverse Modalità

### Metodo 1: Modifica `platformio.ini` (Permanente)

Aggiungi il flag in `platformio.ini`:

```ini
[env:freenove-esp32-s3-display]
platform = espressif32
# ...
build_flags =
  # ... altri flags ...
  -D LVGL_BUFFER_MODE=1  # ← Cambia questo (0, 1, o 2)
```

Poi compila e carica:
```bash
pio run -t upload
```

### Metodo 2: Command Line (Temporaneo)

Testa una modalità senza modificare `platformio.ini`:

```bash
# Test PSRAM Single (modalità 0)
pio run -t upload --build-flag="-D LVGL_BUFFER_MODE=0"

# Test DRAM Single (modalità 1 - default)
pio run -t upload --build-flag="-D LVGL_BUFFER_MODE=1"

# Test DRAM Double (modalità 2)
pio run -t upload --build-flag="-D LVGL_BUFFER_MODE=2"
```

### Metodo 3: Modifica Codice (Manuale)

Modifica direttamente `src/main.cpp`:

```cpp
// Linea ~50
#ifndef LVGL_BUFFER_MODE
  #define LVGL_BUFFER_MODE 2  // ← Cambia qui (0, 1, o 2)
#endif
```

---

## Verifica Modalità Attiva

Dopo il boot, controlla il serial monitor (115200 baud). Cerca queste righe:

```
[LVGL] Allocating DRAM single buffer...
[LVGL] Buffer mode: DRAM Single
[LVGL] Buffer 1: 7680 px (15360 bytes) @ 0x3fcxxxxx [Internal RAM]
```

Oppure per double buffer:

```
[LVGL] Allocating DRAM double buffer...
[LVGL] Buffer mode: DRAM Double
[LVGL] Buffer 1: 7680 px (15360 bytes) @ 0x3fcxxxxx [Internal RAM]
[LVGL] Buffer 2: 7680 px (15360 bytes) @ 0x3fcxxxxx [Internal RAM]
[LVGL] Total RAM used: 30720 bytes
```

---

## Testing Plan

### 1. Baseline (PSRAM)

```bash
pio run -t upload --build-flag="-D LVGL_BUFFER_MODE=0"
pio device monitor
```

**Osserva:**
- Log di boot: verifica "PSRAM Single" mode
- Memoria DRAM dopo boot (dovrebbe essere ~300 KB liberi)
- Animazioni UI: nota eventuali micro-stuttering

**Screenshot:** Salva log di memoria

### 2. DRAM Single (Raccomandato)

```bash
pio run -t upload --build-flag="-D LVGL_BUFFER_MODE=1"
pio device monitor
```

**Osserva:**
- Log di boot: verifica "DRAM Single" mode
- Memoria DRAM dopo boot (~285 KB liberi, -15 KB rispetto a PSRAM)
- Animazioni UI: dovrebbero essere più fluide

**Confronta con baseline:** Noti differenze visibili?

### 3. DRAM Double (Massime Performance)

```bash
pio run -t upload --build-flag="-D LVGL_BUFFER_MODE=2"
pio device monitor
```

**Osserva:**
- Log di boot: verifica "DRAM Double" mode
- Memoria DRAM dopo boot (~270 KB liberi, -30 KB rispetto a PSRAM)
- Animazioni UI: massima fluidità (soprattutto scrolling)

**Confronta con DRAM Single:** Vale la pena usare 15 KB extra?

---

## Benchmark Performance (Opzionale)

### Test DMA Transfer Speed

Puoi aggiungere questo codice nel `DeveloperScreen` per benchmark oggettivo:

```cpp
void benchmarkDmaTransfer() {
    const size_t test_bytes = 15360;  // Dimensione buffer LVGL
    const int iterations = 100;

    // Test con buffer attuale
    uint32_t start = millis();
    for (int i = 0; i < iterations; i++) {
        tft.pushPixelsDMA((uint16_t*)draw_buf_ptr, test_bytes / 2);
        tft.dmaWait();
    }
    uint32_t elapsed = millis() - start;

    float avg_ms = elapsed / float(iterations);
    float bandwidth_mbps = (test_bytes * iterations) / (elapsed * 1000.0);

    Logger::infof("[Benchmark] %d transfers of %u bytes", iterations, test_bytes);
    Logger::infof("[Benchmark] Total: %lu ms, Avg: %.2f ms/transfer", elapsed, avg_ms);
    Logger::infof("[Benchmark] Bandwidth: %.2f MB/s", bandwidth_mbps);
}
```

**Risultati attesi:**
- **PSRAM:** ~0.38 ms/transfer, ~40 MB/s
- **DRAM Single:** ~0.19 ms/transfer, ~80 MB/s
- **DRAM Double:** ~0.19 ms/transfer, ~80 MB/s (stesso di single, ma zero contese durante render)

---

## Troubleshooting

### Errore: "Failed to allocate DRAM buffer"

**Causa:** RAM interna insufficiente (raro, a meno che non ci siano grandi allocazioni precedenti)

**Soluzioni:**
1. Controlla log memoria: `[Memory] DRAM free ...`
2. Se DRAM < 50 KB, riduci stack dei task in `src/core/task_config.h`
3. Oppure usa modalità 0 (PSRAM)

### Errore: "LVGL_BUFFER_MODE deve essere 0, 1 o 2"

**Causa:** Define con valore invalido

**Soluzione:** Verifica che il valore in `platformio.ini` o `main.cpp` sia 0, 1, o 2

### Performance peggiori con DRAM

**Causa:** Possibile frammentazione memoria o altro task che usa RAM interna intensivamente

**Soluzioni:**
1. Riavvia ESP32
2. Controlla log per memory leak
3. Verifica che nessun altro task alloca/dealloca continuamente in DRAM

---

## Raccomandazione Finale

Dopo il testing:

1. **Se DRAM Single (modalità 1) è sufficiente:**
   - Lascia `LVGL_BUFFER_MODE=1` di default
   - Rimuovi le `#if/#elif` se vuoi semplificare il codice

2. **Se serve DRAM Double (modalità 2):**
   - Cambia default a `LVGL_BUFFER_MODE=2`
   - Documenta l'uso di 30 KB RAM nel README

3. **Se preferisci PSRAM (modalità 0):**
   - Cambia default a `LVGL_BUFFER_MODE=0`
   - Ma nota che performance DMA saranno ~50% più lente

**Per la maggior parte dei casi d'uso: DRAM Single (modalità 1) è IDEALE.**

---

## Riferimenti

- **[Analisi Tecnica](lvgl_buffer_analysis.md)** - Dettagli su DMA e PSRAM
- **[Runtime Switching](runtime_buffer_switching_analysis.md)** - Perché non è possibile cambiare a runtime
- **[Task Architecture](task_architecture.md)** - Linee guida performance generali

---

**Ultimo aggiornamento:** 2025-11-22
**Status:** ✅ Implementato e pronto per testing
