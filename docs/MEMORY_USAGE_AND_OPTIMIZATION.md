# Guida all'Uso e Ottimizzazione della Memoria (DRAM & PSRAM)

**Data:** 2025-11-22
**Autore:** Gemini Code Analysis

## 1. Sommario Esecutivo

Questo documento analizza l'utilizzo della memoria **DRAM (interna)** e **PSRAM (esterna)** nel progetto.

L'analisi rivela un'architettura di memoria **già ottimamente configurata**. Il sistema dà priorità all'uso della PSRAM per tutte le allocazioni di grandi dimensioni, riservando la DRAM interna solo per compiti critici in termini di prestazioni e per piccoli oggetti di controllo.

**Non ci sono buffer di grandi dimensioni allocati erroneamente in DRAM** che necessitano di essere spostati in PSRAM. Le ottimizzazioni future dovrebbero concentrarsi sul mantenimento di questa architettura e sulla gestione efficiente della PSRAM disponibile.

---

## 2. Utilizzo Attuale della PSRAM (Memoria Esterna)

La PSRAM è la risorsa di memoria primaria per i dati di grandi dimensioni. Questo è il modo corretto di operare per liberare la preziosa e veloce DRAM interna.

I principali utilizzatori della PSRAM sono:

| Componente | File | Dimensione Stimata | Scopo |
| :--- | :--- | :--- | :--- |
| **Heap Principale LVGL** | `include/lv_psram_allocator.h` | Dinamica | L'intero heap di memoria usato da LVGL per widget, stili e oggetti UI viene allocato qui. È la più grande ottimizzazione del sistema. |
| **Canvas Color Picker** | `src/widgets/circular_color_picker.cpp` | > 150 KB | Un buffer grafico di grandi dimensioni per il rendering della ruota dei colori. |
| **Buffer del Logger** | `src/utils/logger.cpp` | ~17 KB (permanente) | Un ring buffer che memorizza le voci di log più recenti del sistema. |
| **Buffer Schermata Log** | `src/screens/system_log_screen.cpp` | 12 KB (temporaneo) | Un buffer transitorio usato per formattare e visualizzare il contenuto del logger. |
| **Buffer di Disegno LVGL** | `src/main.cpp` | ~15 KB | **Modalità di fallback:** solo se `LVGL_BUFFER_MODE` è impostato su `2`. |

### Strategia di Allocazione
Il codice adotta una strategia robusta di "PSRAM-first":
1.  Tenta di allocare il buffer in **PSRAM** usando `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`.
2.  Se l'allocazione fallisce (ad esempio, per PSRAM esaurita), esegue un **fallback** allocando in **DRAM** (spesso con una dimensione ridotta per non impattare il sistema).

Questa strategia garantisce la resilienza del sistema ma evidenzia l'importanza di non esaurire la PSRAM.

---

## 3. Utilizzo Attuale della DRAM (Memoria Interna)

La DRAM è usata intenzionalmente solo per due scopi principali:

| Componente | File | Dimensione Stimata | Scopo |
| :--- | :--- | :--- | :--- |
| **Buffer di Disegno LVGL** | `src/main.cpp` | 15-30 KB | **Allocazione critica per le prestazioni.** Con `LVGL_BUFFER_MODE=1` (default), i buffer vengono allocati in DRAM per massimizzare la velocità di trasferimento via DMA al display. Spostarli in PSRAM causerebbe un **calo di prestazioni visibili** (stuttering). |
| **Oggetti e Task** | Intero progetto (`src/`) | Dinamica (piccola) | Tutti gli oggetti di classe (`new MyManager()`), le variabili globali, lo stack dei task di FreeRTOS e piccole strutture dati risiedono in DRAM. Questo è il comportamento standard e corretto. |

**Conclusione:** L'uso della DRAM è minimo e giustificato da requisiti di performance.

---

## 4. Opportunità di Ottimizzazione e Raccomandazioni

Il sistema è già ben ottimizzato. Le seguenti non sono correzioni urgenti ma buone pratiche e aree di miglioramento.

### 4.1. Correzione Bug di Deallocazione (Raccomandato)

-   **Problema:** In `src/screens/system_log_screen.cpp`, il codice usa `free()` per deallocare un buffer (`log_buffer`) che potrebbe provenire sia da `heap_caps_malloc` (PSRAM) che da `malloc` (DRAM). Usare `free` su un puntatore ottenuto da `heap_caps_malloc` è tecnicamente scorretto.
-   **Soluzione:** Tracciare l'origine del buffer e usare la funzione di deallocazione corrispondente.

**Esempio di correzione in `system_log_screen.cpp`:**

```cpp
// Aggiungere una variabile membro alla classe SystemLogScreen
bool _log_buffer_is_psram = false;

// ... nella funzione create_log_content()
// Quando si alloca con heap_caps_malloc
_log_buffer_is_psram = true;
log_buffer = (char*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM);

// Quando si alloca con malloc (nel fallback)
_log_buffer_is_psram = false;
log_buffer = (char*)malloc(FALLBACK_BUFFER_SIZE);


// ... nella funzione di pulizia o nel distruttore
if (log_buffer) {
    if (_log_buffer_is_psram) {
        heap_caps_free(log_buffer);
    } else {
        free(log_buffer);
    }
    log_buffer = nullptr;
}
```

### 4.2. Monitoraggio della PSRAM

-   **Raccomandazione:** Poiché il sistema fa molto affidamento sulla PSRAM, è fondamentale assicurarsi che non si esaurisca. Se la PSRAM si esaurisce, i componenti inizieranno a usare la DRAM come fallback, portando a un rapido esaurimento della memoria interna e a possibili crash.
-   **Azione:** Usare le funzioni di `esp_psram_...` o `heap_caps_...` per monitorare l'utilizzo della PSRAM nella schermata "Developer" o tramite log periodici.

```cpp
// Esempio di monitoraggio
size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
size_t psram_min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
Logger::infof("PSRAM: Free=%u, MinFree=%u", psram_free, psram_min_free);
```

### 4.3. Linee Guida per Futuro Sviluppo

Per mantenere il sistema performante, seguire queste regole:

1.  **Buffer > 1 KB? Usa PSRAM.** Qualsiasi buffer di dati (immagini, file, array di grandi dimensioni) deve essere allocato in PSRAM usando `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`.
2.  **Piccoli oggetti di controllo? Usa DRAM.** Per oggetti di stato, manager, o piccole strutture dati, va bene usare `new` o `malloc`, che allocano in DRAM.
3.  **Implementa sempre un Fallback.** Quando allochi in PSRAM, prevedi sempre un percorso di fallback che gestisca il caso in cui l'allocazione fallisca (ad es. allocando un buffer più piccolo in DRAM o disabilitando una feature).
4.  **Non spostare i buffer DMA in PSRAM.** Non modificare la configurazione di `LVGL_BUFFER_MODE` in `main.cpp` senza essere consapevoli che ciò ridurrà le prestazioni grafiche. L'allocazione in DRAM per i buffer di disegno è una scelta deliberata e corretta.

---
## 5. Riferimenti Chiave nel Codice

-   **`include/lv_psram_allocator.h`**: Configura LVGL per usare l'heap in PSRAM.
-   **`src/main.cpp`**: (Vedi `setupLVGL`) Alloca i buffer di disegno LVGL in DRAM.
-   **`src/utils/logger.cpp`**: (Vedi costruttore `Logger`) Esempio di allocazione PSRAM-first con fallback in DRAM.
-   **`src/widgets/circular_color_picker.cpp`**: (Vedi `createCircularColorPicker`) Esempio di allocazione di un buffer grafico di grandi dimensioni in PSRAM.
