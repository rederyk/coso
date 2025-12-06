# 📋 Sistema di Variabili Dinamiche - Guida d'Uso

## ✨ Panoramica

Il sistema di prompt dinamico ora mostra **in tempo reale** le variabili attive e i loro valori nella WebUI. Le variabili vengono evidenziate sia nel prompt che in una sezione dedicata.

---

## 🎯 Funzionalità Implementate

### 1. **Variabili Automatiche**
Dopo ogni esecuzione di comando, queste variabili vengono automaticamente create:

- `{{last_command_name}}` - Nome dell'ultimo comando eseguito
- `{{last_command_text}}` - Risposta testuale dell'LLM
- `{{last_command_raw_output}}` - Output grezzo del comando
- `{{last_command_refined_output}}` - Output processato (se disponibile)
- `{{command_<nome>_output}}` - Output specifico per comando
- `{{command_<nome>_refined_output}}` - Output processato specifico

### 2. **Visualizzazione WebUI**

#### Sezione "Variabili Dinamiche Attive"
- Appare automaticamente sotto l'anteprima del prompt
- Mostra tutte le variabili attive con i loro valori
- **Evidenziazione:** variabili nel prompt mostrate con sfondo giallo
- **Click per copiare:** clicca sul nome di una variabile per copiare `{{nome}}`

#### Aggiornamento Automatico
- Le variabili si aggiornano automaticamente dopo ogni comando
- L'anteprima del prompt si ricarica mostrando i valori correnti

---

## 📝 Esempi Pratici

### Esempio 1: Meteo Automatico nel Prompt

**Nel JSON del prompt:**
```json
{
  "prompt_template": "...",
  "sections": [
    "**METEO CORRENTE:** {{command_lua_script_refined_output}}",
    "Usa questi dati per rispondere alle domande sul meteo."
  ]
}
```

**Flusso:**
1. Utente chiede: "Che tempo fa a Milano?"
2. LLM esegue: `lua_script` con fetch meteo
3. Sistema cattura output in `{{command_lua_script_output}}`
4. Output viene raffinato in `{{command_lua_script_refined_output}}`
5. **WebUI mostra** la variabile con il valore JSON del meteo
6. Prossima richiesta: prompt contiene già i dati meteo!

### Esempio 2: File dalla SD Card

**Nel JSON:**
```json
{
  "sections": [
    "**CONFIGURAZIONE DISPOSITIVO:** {{command_lua_script_output}}",
    "Il file config.json contiene le impostazioni attuali."
  ]
}
```

**Comandi:**
```javascript
// L'utente chiede: "Leggi il file di configurazione"
// LLM esegue:
memory.read_file('/config.json')

// Output salvato in {{command_lua_script_output}}
// Visibile nella WebUI sotto "Variabili Dinamiche Attive"
```

### Esempio 3: Stato Sistema in Tempo Reale

**Nel JSON:**
```json
{
  "sections": [
    "**STATO SISTEMA:**",
    "- Heap: {{command_heap_output}}",
    "- Uptime: {{command_uptime_output}}",
    "- SD Status: {{command_sd_status_output}}"
  ]
}
```

**Ogni comando salva il suo output** in una variabile dedicata, visibile nella WebUI.

---

## 🔧 Come Usare nella WebUI

### Passo 1: Vai su Settings
Apri la pagina delle impostazioni nella WebUI

### Passo 2: Modifica il JSON del Prompt
Nella sezione "Prompt della voce", aggiungi variabili nel formato `{{nome_variabile}}`

### Passo 3: Osserva l'Anteprima
- Variabili **evidenziate in giallo** se hanno un valore
- Sezione "⚡ Variabili Dinamiche Attive" mostra tutti i valori

### Passo 4: Esegui Comandi
- Invia un messaggio che esegue un comando (es. "leggi il meteo")
- **Le variabili si aggiornano automaticamente**
- L'anteprima si ricarica con i nuovi valori

### Passo 5: Click per Copiare
Clicca su una variabile nella sezione per copiare `{{nome}}` negli appunti

---

## 🎨 Stili Visuali

### Evidenziazione nel Prompt
```
Sfondo: giallo trasparente (rgba(255, 235, 59, 0.15))
Colore testo: #ffd54f
Padding: 2px 6px
Border-radius: 4px
```

### Card Variabile
```
Sfondo: verde acqua trasparente
Bordo: verde acqua semi-trasparente
Font: monospace (JetBrains Mono/Space Mono)
Click: copia negli appunti + feedback visivo
```

---

## 🚀 Casi d'Uso Avanzati

### 1. **Dashboard Integrata nel Prompt**
```json
{
  "sections": [
    "**DASHBOARD DISPOSITIVO:**",
    "```",
    "Memoria: {{command_heap_output}}",
    "Rete: {{command_ping_output}}",
    "Storage: {{command_sd_status_output}}",
    "```"
  ]
}
```

### 2. **Context-Aware Assistant**
```json
{
  "sections": [
    "Hai accesso a questi dati aggiornati:",
    "- Ultimo comando: {{last_command_name}}",
    "- Output: {{last_command_refined_output}}",
    "Usa queste info per rispondere in modo contestuale."
  ]
}
```

### 3. **Multi-Source Data Aggregation**
```json
{
  "sections": [
    "**DATI AGGREGATI:**",
    "Meteo: {{command_webdata_read_data_refined_output}}",
    "Log: {{command_log_tail_refined_output}}",
    "Sistema: {{command_system_status_refined_output}}"
  ]
}
```

---

## 🔍 Debug e Troubleshooting

### Le variabili non appaiono?
1. Verifica che il comando venga effettivamente eseguito (guarda i log)
2. Controlla che il comando produca output (non tutti i comandi lo fanno)
3. Ricarica la pagina WebUI

### L'anteprima non si aggiorna?
- Le variabili si aggiornano solo **dopo l'esecuzione di comandi**
- Clicca "Ricarica JSON" per forzare l'aggiornamento

### Valori troncati?
- Valori lunghi (>500 caratteri) vengono troncati con "..."
- Il valore completo è comunque usato nel prompt inviato all'LLM

---

## 📊 API Endpoints

### GET `/api/assistant/prompt/variables`
Restituisce tutte le variabili dinamiche correnti

**Response:**
```json
{
  "status": "success",
  "variables": {
    "last_command_name": "lua_script",
    "last_command_raw_output": "{\"temperature\": 15.5}",
    "command_lua_script_output": "{\"temperature\": 15.5}"
  }
}
```

### POST `/api/assistant/prompt/preview`
Ora include le variabili nella risposta

**Response:**
```json
{
  "status": "success",
  "resolvedPrompt": "Testo del prompt risolto...",
  "variables": {
    "last_command_name": "heap",
    "command_heap_output": "128KB free"
  }
}
```

---

## 🎓 Best Practices

1. **Nomi Descrittivi**: Usa nomi variabili chiari (es. `meteo_milano` invece di `data1`)
2. **Struttura**: Organizza le variabili per categoria nel prompt
3. **Fallback**: Gestisci il caso di variabili vuote nel testo del prompt
4. **Lunghezza**: Limita la dimensione totale per evitare prompt troppo lunghi
5. **Refresh**: Esegui comandi periodici per mantenere dati freschi

---

## 📌 Note Tecniche

- Le variabili sono **thread-safe** (protette da mutex)
- Vengono **salvate in memoria RAM** (si perdono al riavvio)
- L'aggiornamento WebUI avviene **via polling dopo comandi**
- Valori massimi: 500 caratteri nell'anteprima, illimitati nel prompt reale

---

**✨ Buon divertimento con il tuo sistema di prompt dinamico!**
