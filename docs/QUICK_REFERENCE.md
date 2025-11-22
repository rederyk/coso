# Quick Reference: Buffer LVGL

## Cambio Modalità Buffer

**File:** `platformio.ini` linea 43

```ini
-D LVGL_BUFFER_MODE=2  # ← Cambia questo (0, 1, o 2)
```

## Modalità

| Modo | RAM | Performance | Usa Quando |
|------|-----|-------------|------------|
| **0** | 0 KB | ⚠️ Media | RAM interna scarsa |
| **1** | 15 KB | ✅ Alta | Default bilanciato |
| **2** | 30 KB | ✅ Ottima | Massime performance |

## Default Attuale

**Modalità 2** (DRAM Double) - 30 KB RAM, massime performance

## Verifica Attiva

Dopo boot, cerca nel serial monitor:

```
[LVGL] Buffer mode: DRAM Double
[LVGL] Buffer 1: 7680 px (15360 bytes) @ 0x3fcXXXXX [Internal RAM]
[LVGL] Buffer 2: 7680 px (15360 bytes) @ 0x3fcYYYYY [Internal RAM]
```

## Memoria DRAM Attesa

- Modalità 0: ~171 KB liberi
- Modalità 1: ~155 KB liberi
- Modalità 2: ~139 KB liberi

---

**Dettagli tecnici:** [lvgl_buffer_analysis.md](lvgl_buffer_analysis.md)
