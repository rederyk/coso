# 🚀 Guida Rapida: Testare i Prompt con Variabili Dinamiche

## 📦 File Disponibili

Ho creato 3 prompt di esempio per te:

1. **`esempio_prompt_dinamico.json`** - Versione base con esempi
2. **`prompt_esempio_comandi_utente.json`** - Versione dettagliata in inglese con tutti i comandi
3. **`prompt_italiano_comandi.json`** - Versione compatta in italiano (CONSIGLIATO)

---

## 🎯 Test Rapido (5 minuti)

### Passo 1: Carica il Firmware
```bash
cd /home/reder/MyTemp/coso
~/.platformio/penv/bin/pio run --target upload
```

### Passo 2: Apri la WebUI
1. Trova l'IP dell'ESP32 (guarda il monitor seriale o il tuo router)
2. Apri browser: `http://<IP_ESP32>/`

### Passo 3: Carica il Prompt Italiano
1. Nella WebUI, clicca su **"Settings"** in alto
2. Scorri fino alla sezione **"Prompt della voce"**
3. Apri il file `prompt_italiano_comandi.json`
4. Copia tutto il contenuto
5. Incolla nell'editor nella WebUI
6. Clicca **"Salva prompt"**

### Passo 4: Osserva l'Anteprima
Subito sotto l'editor dovresti vedere:
- **Anteprima risolta:** il prompt completo
- Se hai già eseguito comandi, vedrai **⚡ Variabili Dinamiche Attive**

---

## 🧪 Scenari di Test

### Test 1: Controllo Memoria (Facile)
**Torna alla pagina principale della chat**

👤 **Scrivi:** "Quanta memoria è libera?"

**Cosa aspettarsi:**
1. ✅ LLM risponde con un messaggio come "Controllo la memoria..."
2. ✅ Esegue il comando `heap()`
3. ✅ Mostra il risultato (es. "128KB free")
4. ✅ **Vai su Settings** → Nella sezione variabili vedi:
   ```
   {{last_command_name}}
   lua_script

   {{command_heap_output}}
   128KB free
   ```

### Test 2: File sulla SD (Medio)
👤 **Scrivi:** "Quali file ho sulla SD?"

**Cosa aspettarsi:**
1. ✅ LLM esegue `memory.list_files()`
2. ✅ Mostra elenco file
3. ✅ **Variabile popolata:** `{{command_lua_script_output}}`
4. ✅ **Nella WebUI Settings** vedi il JSON con la lista file

**Poi prova:**
👤 **Scrivi:** "Salva una nota: comprare il pane"

**Cosa aspettarsi:**
1. ✅ LLM esegue `memory.write_file()`
2. ✅ Conferma salvataggio
3. ✅ **Variabile aggiornata**

👤 **Scrivi:** "Quali file ho ora?" (ricontrolla)
- Dovresti vedere il nuovo file `note.txt` nella lista!

### Test 3: Meteo (Avanzato)
👤 **Scrivi:** "Che tempo fa a Milano?"

**Cosa aspettarsi:**
1. ✅ LLM costruisce URL Open-Meteo
2. ✅ Scarica dati meteo
3. ✅ Elabora output con `should_refine_output: true`
4. ✅ Mostra temperatura e condizioni
5. ✅ **Vai su Settings** → Vedi:
   ```
   {{command_lua_script_refined_output}}
   A Milano ci sono 15.5°C, cielo parzialmente nuvoloso...

   {{command_lua_script_output}}
   {"temperature":15.5,"weather_code":2,...}
   ```

**Poi testa la persistenza:**
👤 **Scrivi:** "E domani che tempo farà?"

**Cosa aspettarsi:**
- ✅ LLM dovrebbe **riutilizzare** i dati già scaricati!
- ✅ Non dovrebbe scaricare di nuovo (se i dati includono previsioni)

### Test 4: Stato Sistema Completo
👤 **Scrivi:** "Come sta il sistema?"

**Cosa aspettarsi:**
1. ✅ LLM esegue `system_status()`
2. ✅ Ottiene JSON con heap, uptime, WiFi, SD
3. ✅ Output raffinato in formato leggibile
4. ✅ **Nella WebUI** vedi dashboard completa

**Poi prova:**
👤 **Scrivi:** "C'è spazio sulla SD?"

**Cosa aspettarsi:**
- ✅ LLM estrae info SD dai dati già scaricati
- ✅ Non esegue un nuovo comando
- ✅ Risponde usando `{{last_command_refined_output}}`

---

## 🔍 Come Verificare che Funzioni

### ✅ Checklist Visiva

Dopo ogni comando, vai su **Settings** e verifica:

1. **Sezione "Variabili Dinamiche Attive" è visibile?**
   - ✅ Sì → Perfetto!
   - ❌ No → Ricarica la pagina

2. **Le variabili mostrano i valori corretti?**
   - ✅ Vedi il nome del comando in `{{last_command_name}}`
   - ✅ Vedi l'output in `{{last_command_output}}`
   - ✅ Se raffinato, vedi testo leggibile in `{{..._refined_output}}`

3. **Nell'anteprima del prompt, le variabili sono evidenziate in giallo?**
   - ✅ Sì → Le variabili attive sono visibili
   - ❌ No → Potrebbero essere vuote

4. **Puoi cliccare sul nome di una variabile e copiarla?**
   - ✅ Click → variabile copiata negli appunti
   - Feedback visivo: il testo diventa verde per 1 secondo

---

## 🐛 Troubleshooting

### Problema: "Variabili non appaiono"
**Cause possibili:**
1. Il comando non è stato eseguito (controlla la risposta dell'LLM)
2. Il comando non produce output (`println()` mancante)
3. Cache WebUI vecchia

**Soluzioni:**
```bash
# 1. Verifica che il comando venga eseguito
# Guarda la chat principale - vedi una risposta?

# 2. Ricarica la pagina WebUI
Ctrl+F5 (force reload)

# 3. Controlla il monitor seriale
~/.platformio/penv/bin/pio device monitor
# Cerca messaggi come "[VoiceAssistant] Captured output: ..."
```

### Problema: "L'anteprima non si aggiorna"
**Soluzione:**
1. Clicca il pulsante **"Ricarica JSON"**
2. Oppure modifica qualcosa nel JSON e risalva
3. Le variabili si aggiornano solo dopo l'esecuzione di comandi

### Problema: "Valori troncati con ..."
**Normale!**
- Valori >500 caratteri vengono troncati **solo nell'anteprima**
- Il prompt reale inviato all'LLM contiene il valore completo
- Questo è per evitare che la WebUI si blocchi con output enormi

---

## 📊 Monitor Seriale (Debug Avanzato)

Per vedere cosa succede nel backend:

```bash
~/.platformio/penv/bin/pio device monitor
```

**Cerca questi messaggi:**
```
[VoiceAssistant] Executing command: lua_script
[VoiceAssistant] Command output: {...}
[VoiceAssistant] Output refinement enabled
[VoiceAssistant] Refined output: <testo leggibile>
[VoiceAssistant] Captured output variables
```

---

## 🎨 Personalizzazione

### Modifica i Prompt

Puoi modificare i prompt direttamente nella WebUI:

1. **Aggiungi nuove sezioni con variabili:**
```json
{
  "sections": [
    "**IL MIO DASHBOARD:**",
    "Memoria: {{command_heap_output}}",
    "Meteo: {{command_lua_script_refined_output}}"
  ]
}
```

2. **Crea variabili custom:**
   - Le variabili si creano automaticamente quando esegui comandi
   - Usa nomi descrittivi nei tuoi script Lua

3. **Evidenziazione automatica:**
   - Qualsiasi `{{nome_variabile}}` nel prompt viene evidenziato
   - Se la variabile ha un valore, appare nella sezione dedicata

---

## 🎯 Test Avanzati

### Test Conversazione Multi-Step

**Scenario: Dashboard Completa**

```
👤 "Controlla il meteo a Milano"
   → Output in {{command_lua_script_refined_output}}

👤 "Quanta memoria ho?"
   → Output in {{command_heap_output}}

👤 "Da quanto sei acceso?"
   → Output in {{command_uptime_output}}
```

**Ora vai su Settings e dovresti vedere TUTTE E TRE le variabili!**

### Test Persistenza Dati

```
👤 "Salva una nota: meeting alle 15:00"
   → File salvato su SD

👤 "Cosa devo ricordare?"
   → LLM legge il file e risponde

👤 "Cancella la nota"
   → memory.delete_file()
```

---

## 📝 Note Finali

### Performance
- Le variabili sono in RAM (volatili, si perdono al riavvio)
- Thread-safe con mutex
- Overhead minimo (<1KB per variabile)

### Limiti
- Variabili vuote non appaiono nella sezione dedicata
- Valori >500 caratteri troncati solo nella WebUI
- No persistenza tra riavvii (by design)

### Best Practices
1. ✅ Usa `println()` sempre per mostrare output
2. ✅ Abilita `should_refine_output: true` per JSON
3. ✅ Dai nomi descrittivi alle variabili (es. `meteo_milano`)
4. ✅ Riutilizza dati esistenti invece di riscaricare

---

## 🚀 Prossimi Passi

Una volta che tutto funziona:

1. **Crea il tuo prompt personalizzato**
   - Usa `prompt_italiano_comandi.json` come base
   - Aggiungi le tue sezioni custom
   - Definisci quali variabili mostrare

2. **Integra con i tuoi workflow**
   - Scarica dati che ti servono spesso
   - Salvali in variabili persistenti
   - Riferisciti ad essi nelle conversazioni

3. **Condividi**
   - Esporta il tuo prompt JSON
   - Condividilo con altri utenti
   - Contribuisci con esempi

---

**✨ Buon divertimento con le variabili dinamiche!**

Se qualcosa non funziona, controlla:
1. Console browser (F12) per errori JavaScript
2. Monitor seriale per errori backend
3. File di log sulla SD card (se abilitati)
