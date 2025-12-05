# Calendar API - Riferimento Rapido

## Comandi Disponibili

### 📋 Listare Eventi
```
calendar_list()
```
Mostra tutti gli eventi (one-shot e ricorrenti) con ID, nome, orario, status.

### ⏰ Creare Allarme (One-Shot)
```
calendar_create_alarm(name, date, time, lua_script)
```
- **name**: Nome evento (stringa)
- **date**: Formato `YYYY-MM-DD` (es: `2025-12-05`)
- **time**: Formato `HH:MM` (es: `07:30`)
- **lua_script**: Script Lua da eseguire

**Esempio:**
```json
{
  "command": "calendar_create_alarm",
  "args": ["Sveglia Mattina", "2025-12-06", "07:00", "println('Buongiorno!')"]
}
```

### 🔄 Creare Evento Ricorrente
```
calendar_create_recurring(name, time, weekdays_mask, lua_script)
```
- **name**: Nome evento
- **time**: Formato `HH:MM`
- **weekdays_mask**: Maschera bit giorni (vedi sotto)
- **lua_script**: Script Lua da eseguire

**Esempio:**
```json
{
  "command": "calendar_create_recurring",
  "args": ["Allarme Lavoro", "08:00", "62", "println('Ora di lavorare!')"]
}
```

### 🗑️ Eliminare Evento
```
calendar_delete(event_id)
```
Elimina evento permanentemente. Richiede ID da `calendar_list()`.

### ⏸️ Abilitare/Disabilitare
```
calendar_enable(event_id, "true"|"false")
```
Disabilita temporaneamente senza eliminare.

### ▶️ Eseguire Subito
```
calendar_run(event_id)
```
Esegue evento immediatamente senza aspettare orario programmato (utile per test).

### 📜 Storico Esecuzioni
```
calendar_history([event_id])
```
Mostra ultime 10 esecuzioni (tutte o di evento specifico).

---

## Weekdays Mask - Riferimento

### Valori Base
| Giorno | Bit | Valore |
|--------|-----|--------|
| Domenica | 0 | 1 |
| Lunedì | 1 | 2 |
| Martedì | 2 | 4 |
| Mercoledì | 3 | 8 |
| Giovedì | 4 | 16 |
| Venerdì | 5 | 32 |
| Sabato | 6 | 64 |

### Pattern Comuni
| Descrizione | Mask | Binary | Calcolo |
|-------------|------|--------|---------|
| **Tutti i giorni** | `127` | `0b1111111` | 1+2+4+8+16+32+64 |
| **Lun-Ven** | `62` | `0b0111110` | 2+4+8+16+32 |
| **Weekend** | `65` | `0b1000001` | 1+64 |
| **Solo Lunedì** | `2` | `0b0000010` | 2 |
| **Solo Domenica** | `1` | `0b0000001` | 1 |
| **Lun-Mer-Ven** | `42` | `0b0101010` | 2+8+32 |
| **Mar-Gio** | `20` | `0b0010100` | 4+16 |
| **Lun-Mer** | `10` | `0b0001010` | 2+8 |

### Come Calcolare
**Esempio**: Voglio Martedì + Giovedì + Sabato
1. Martedì = 4
2. Giovedì = 16
3. Sabato = 64
4. **Totale = 4 + 16 + 64 = 84**

---

## Script Lua - API Disponibili

### GPIO
```lua
gpio.write(23, true)      -- Accendi GPIO 23
gpio.write(23, false)     -- Spegni GPIO 23
gpio.toggle(23)           -- Inverti stato
```

### Delay
```lua
delay(1000)               -- Attesa 1 secondo (1000ms)
```

### Output
```lua
println('Messaggio')      -- Stampa output (visibile in storico)
```

### Radio/Audio
```lua
radio.play('http://stream.url/radio.mp3')  -- Avvia streaming
radio.play('/sd/music/song.mp3')           -- File locale
radio.stop()                               -- Ferma
radio.pause()                              -- Pausa
radio.resume()                             -- Riprendi
radio.set_volume(75)                       -- Volume 0-100
```

### Web Data
```lua
webData.fetch_once('https://api.url.com/data', 'output.json')  -- Scarica
local data = webData.read_data('output.json')                  -- Leggi
```

### Memory/File
```lua
memory.write_file('path/file.txt', 'contenuto')  -- Scrive
local content = memory.read_file('path/file.txt') -- Legge
memory.append_file('log.txt', 'nuova riga\n')    -- Appendi
```

### BLE Keyboard
```lua
ble.type('Testo da digitare')   -- Digita testo
ble.send_key('RETURN')          -- Tasti speciali
```

### System
```lua
local ok, time = uptime()       -- Secondi accensione
local ok, heap = heap()         -- Memoria disponibile
local ok, status = system.status() -- Status completo
```

### Documentazione
```lua
local cities = docs.reference.cities()  -- Legge cities.json
local api = docs.api.gpio()             -- Legge API GPIO
```

---

## Template Script Comuni

### LED Lampeggiante
```lua
for i=1,5 do
  gpio.toggle(23);
  delay(500);
end;
println('Lampeggio completato')
```

### Controllo Meteo
```lua
local c=docs.reference.cities();
webData.fetch_once(
  'https://api.open-meteo.com/v1/forecast?latitude='..c.Milano.lat..
  '&longitude='..c.Milano.lon..'&current=temperature_2m',
  'weather.json'
);
println('Meteo scaricato')
```

### Accendi/Spegni GPIO
```lua
gpio.write(23, true);
println('GPIO 23 acceso')
```

### Avvia Radio
```lua
radio.play('http://stream.url.com/radio.mp3');
delay(2000);
radio.set_volume(50);
println('Radio avviata al 50%')
```

### Backup Configurazioni
```lua
local cfg = system.status();
memory.write_file('backup_config.json', cfg);
println('Backup salvato')
```

### Notifica BLE
```lua
ble.type('Promemoria importante');
delay(100);
ble.send_key('RETURN');
println('Notifica inviata via BLE')
```

---

## Esempi Conversazioni IA

### Sveglia Semplice
**User**: "Crea una sveglia per domani alle 7"

**IA**:
```json
{
  "command": "calendar_create_alarm",
  "args": ["Sveglia Mattina", "2025-12-06", "07:00", "println('Buongiorno!')"],
  "text": "Ho creato una sveglia per domani 6 dicembre alle 7:00"
}
```

### Allarme Ricorrente Settimanale
**User**: "Allarme ogni giorno lavorativo alle 8"

**IA**:
```json
{
  "command": "calendar_create_recurring",
  "args": ["Allarme Lavoro", "08:00", "62", "for i=1,5 do gpio.toggle(23);delay(500);end;println('Ora di lavorare!')"],
  "text": "Ho creato un allarme Lun-Ven alle 8:00 con LED lampeggiante"
}
```

### Automazione Casa
**User**: "Accendi le luci ogni sera alle 19"

**IA**:
```json
{
  "command": "calendar_create_recurring",
  "args": ["Luci Serali", "19:00", "127", "gpio.write(23,true);println('Luci accese')"],
  "text": "Ho programmato accensione luci alle 19:00 ogni giorno"
}
```

### Promemoria Medicina
**User**: "Ricordami medicina alle 8 mattina e sera"

**IA** (multiplo):
```json
[
  {
    "command": "calendar_create_recurring",
    "args": ["Medicina Mattina", "08:00", "127", "println('💊 Medicina mattutina!')"]
  },
  {
    "command": "calendar_create_recurring",
    "args": ["Medicina Sera", "20:00", "127", "println('💊 Medicina serale!')"]
  }
]
```

---

## Best Practices

### ✅ DO
- Usa `calendar_list()` prima di eliminare per trovare ID corretto
- Usa `calendar_enable(id, false)` per disabilitazione temporanea
- Testa script con `calendar_run(id)` prima di aspettare orario
- Usa `println()` negli script per output visibile in storico
- Per eventi ricorrenti usa weekdays mask comuni (127, 62, 65)
- Script brevi e compatti (evita multi-line complessi)

### ❌ DON'T
- Non eliminare evento se vuoi solo disabilitarlo temporaneamente
- Non usare date passate per one-shot alarm
- Non dimenticare di calcolare weekdays mask per recurring
- Non creare script troppo lunghi (timeout 10s)
- Non usare weekdays mask = 0 (nessun giorno selezionato)

---

## Troubleshooting

### Evento non eseguito
- Verifica che sia `enabled: true`
- Controlla `next_run` timestamp con `calendar_list()`
- Verifica che TimeManager sia sincronizzato
- Usa `calendar_history()` per vedere errori

### Script Lua fallisce
- Controlla sintassi Lua (virgole, parentesi)
- Verifica timeout (max 10s esecuzione)
- Testa con `calendar_run(id)` per vedere output
- Leggi `last_error` in `calendar_list()`

### Weekdays mask non funziona
- Verifica calcolo: somma valori giorni corretti
- Usa pattern comuni (127=tutti, 62=Lun-Ven, 65=weekend)
- Non usare mask = 0

---

## Limitazioni

- ⏱️ Precisione ±30 secondi (check ogni 30s)
- ⏲️ Timeout script Lua: 10 secondi
- 📦 Storico: max 100 esecuzioni
- 💾 Eventi salvati in `/calendar_events.json`
- 🔄 One-shot marcati 'completed' dopo esecuzione
- 🔁 Recurring rimangono 'pending' e si ripetono

---

## Accesso Documentazione da Lua

```lua
-- L'IA può leggere docs per costruire script corretti
local calendar_api = docs.api.calendar()
local scenarios = docs.examples.calendar_scenarios()
local cities = docs.reference.cities()
```
