# MemoryManager Implementation Plan

## 🎯 Obiettivo

Creare un nuovo `MemoryManager` dedicato alla gestione della memoria del dispositivo con permessi granulari per cartella, separando questa funzionalità da `WebDataManager` che rimane focalizzato sui dati web.

## 📋 Analisi della Situazione Attuale

### WebDataManager (Attuale)
- ✅ Gestisce downloads HTTP/web
- ✅ Gestisce caching in `/webdata`
- ❌ Gestisce anche lettura/scrittura `/memory` (responsabilità mista)

### StorageAccessManager (Attuale)
- ✅ Whitelist basate su path per SD/LittleFS
- ❌ Permessi semplificati (lettura/scrittura globale per path)
- ❌ Nessuna distinzione tra lettura/scrittura/cancellazione

## 🏗️ Design del Nuovo MemoryManager

### Responsabilità
- **Gestione esclusiva della memoria del dispositivo** (`/memory`, `/userDir`, etc.)
- **Permessi granulari per directory**:
  - Lettura (read)
  - Scrittura (write)
  - Cancellazione (delete)
- **Integrazione con StorageAccessManager** per validazione path
- **API semplice e sicura** per voice assistant

### Struttura Directory Whitelist

```cpp
struct DirectoryPermissions {
    std::string path;
    bool can_read;
    bool can_write;
    bool can_delete;
};

std::vector<DirectoryPermissions> directory_permissions_;
```

### Esempio Configurazione
```json
{
  "memory": {
    "directories": {
      "/memory": {"read": true, "write": true, "delete": false},
      "/userDir": {"read": true, "write": true, "delete": true}
    }
  }
}
```

## 🔄 Pianificazione Implementazione

### Fase 1: Core MemoryManager
1. **Creare header** `include/core/memory_manager.h`
   - Definire `DirectoryPermissions` struct
   - Interfaccia principale con metodi: `readData()`, `writeData()`, `listFiles()`, `deleteData()`
   - Metodo `getDirectoryPermissions()` per configurazione

2. **Implementare** `src/core/memory_manager.cpp`
   - Inizializzazione con caricamento permessi da settings
   - Metodi CRUD con validazione permessi
   - Integrazione con `StorageAccessManager` per path validation
   - Logging e error handling

### Fase 2: Estensioni StorageAccessManager
1. **Aggiornare SettingsManager**
   - Aggiungere configurazione permessi granulari nelle impostazioni
   - Default permissions per `/memory` (read/write, no delete)

2. **Modificare StorageAccessManager**
   - Aggiungere supporto per permessi read/write/delete per path
   - Metodo `checkPermissions(path, permission)` per validazione
   - Backward compatibility con whitelist esistenti

### Fase 3: Separazione WebDataManager
1. **Rimuovere metodi memory-related** da WebDataManager:
   - `readMemory()`, `writeMemory()` → spostare in MemoryManager
   - Aggiornare solo metodi web-specific: `fetchOnce()`, `fetchScheduled()`, lettura `/webdata`

2. **Aggiornare WebDataManager** per essere stateless riguardo memory:
   - Rimuovere dipendenze da `/memory` directory
   - Focus esclusivo su `/webdata` per caching web

### Fase 4: Integrazione API Lua
1. **Aggiornare Voice Assistant Lua Sandbox**
   - Sostituire API memory in `webData.*` con nuove `memory.*` API
   - Esempio:
     ```lua
     -- Vecchio (corretto ma contro architettura)
     webData.read_data('README.md')

     -- Nuovo (corretta separazione)
     memory.read_data('README.md')
     ```

2. **Nuove API Lua per MemoryManager**:
   - `memory.read_file(filename)` - lettura file
   - `memory.write_file(filename, data)` - scrittura file
   - `memory.list_files()` - lista file in memoria
   - `memory.delete_file(filename)` - cancellazione file (se permesso)

### Fase 5: Aggiornamenti Settings e Configurazione
1. **Aggiornare `data/settings.json`**
   - Aggiungere sezione `memory.directories` con permessi
   - Rimuovere configurazione memory da `web_data_manager`

2. **Aggiornare StorageManager**
   - Supportare caricamento/salvataggio permessi granulari
   - Migrazione automatica da vecchie impostazioni

### Fase 6: System Prompt e Documentazione
1. **Aggiornare Voice Assistant Prompt**
   - Descrivere separazione MemoryManager/WebDataManager
   - API Lua aggiornate con esempi corretti
   - Warning sui permessi directory-specific

2. **Documentazione**
   - Aggiornare commenti codice
   - Creare documentazione API per MemoryManager
   - Esempi di configurazione permessi

### Fase 7: Testing e Migrazione
1. **Test Unitari**
   - Test permessi granulari per directory
   - Test backward compatibility
   - Test sicurezza (non accesso a path non autorizzati)

2. **Test Integrazione**
   - Voice assistant può accedere correttamente a memoria
   - WebDataManager funziona ancora per web downloads
   - Nessuna breaking change per API esistenti

3. **Migrazione**
   - Script di migrazione automatica per dati esistenti in `/memory`
   - Aggiornamento automatico delle impostazioni

## ⚡ Vantaggi del Nuovo Design

### Sicurezza
- **Permessi granulari**: controllo preciso read/write/delete per directory
- **Separazione chiara**: web data ≠ device memory
- **Validazione obbligatoria**: ogni operazione richiede permesso esplicito

### Manutenibilità
- **Single Responsibility**: ogni manager ha una responsabilità chiara
- **API chiare**: memory vs web data sono distinti
- **Configurazione centralizzata**: permessi gestiti in settings

### Estensibilità
- **Nuove directory**: facile aggiungere nuove aree memoria con permessi specifici
- **Policy flessibili**: supporto futuri tipi di permessi (es. execute per script)
- **Integrazione semplice**: altri componenti possono usare MemoryManager

## 🔄 Stato Implementazione

### ✅ Completato
- [x] Analisi architetturale e piano dettagliato
- [x] Identificazione problemi attuale design
- [x] Definizione interfaccia MemoryManager

### 🚧 In Corso
- [ ] Implementazione MemoryManager header
- [ ] Implementazione MemoryManager core
- [ ] Estensioni StorageAccessManager

### ⏳ Pianificato
- [ ] Separazione WebDataManager
- [ ] Aggiornamento API Lua
- [ ] Aggiornamento settings e configurazione
- [ ] System prompt e documentazione
- [ ] Testing e migrazione

## 📊 Timeline Stimata
- **Fase 1-2**: 2-3 giorni (implementazione core)
- **Fase 3-4**: 2 giorni (integrazione)
- **Fase 5-6**: 1 giorno (settings e documentazione)
- **Fase 7**: 1 giorno (testing)

**Totale**: ~1 settimana per implementazione completa
