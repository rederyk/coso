# ESP32-S3 Touch Display OS

Firmware avanzato per Freenove ESP32-S3 con display touch 2.8" (320×240).

## Licenza

Apache-2.0. Vedi `LICENSE` e `NOTICE` per dettagli e attribution di terze parti.

## 🚀 Quick Start

```bash
platformio run -t upload
platformio device monitor
```

## ⚙️ Configurazione Buffer LVGL

Il progetto supporta **3 modalità di buffer** per ottimizzare le performance in base alle esigenze.

**Modifica il file `platformio.ini` linea 43:**

```ini
build_flags =
  # ... altri flags ...
  -D LVGL_BUFFER_MODE=1  # ← Cambia questo numero (0, 1, o 2)
```

### Modalità Disponibili

| Modo | Nome | RAM Usata | Performance | Quando Usarlo |
|------|------|-----------|-------------|---------------|
| **0** | PSRAM Single | 0 KB DRAM | ⚠️ Media | Device con poca RAM interna |
| **1** | DRAM Single | 15 KB DRAM | ✅ Alta | **Default raccomandato** |
| **2** | DRAM Double | 30 KB DRAM | ✅ Ottima | Animazioni complesse, video |

**Default attuale:** Modalità **2** (DRAM Double) - massime performance

### Quando Cambiare Modalità

- **Usa modo 0 (PSRAM):** Solo se hai meno di 50 KB RAM interna libera
- **Usa modo 1 (DRAM Single):** Configurazione bilanciata (raccomandato per la maggior parte dei casi)
- **Usa modo 2 (DRAM Double):** Quando vuoi le massime performance e hai RAM disponibile

---

## 📚 Documentazione

### Quick Reference
- **[QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md)** - Riferimento rapido cambio modalità buffer

### Architettura
- **[task_architecture.md](docs/task_architecture.md)** - Core affinity, priorità task, pattern thread-safe
- **[lvgl_buffer_analysis.md](docs/lvgl_buffer_analysis.md)** - Analisi tecnica buffer LVGL e DMA
