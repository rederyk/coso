# 📅 Calendar Scheduler - AI Prompt Examples

## Sezione da Aggiungere a `prompt_italiano_comandi.json`

Inserire dopo la sezione **GPIO** (line ~170):

```json
"### 📅 Calendar & Alarms\n",
"**Utente:** \"Crea una sveglia per domani alle 7\"\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_create_alarm\",\n",
"  \"args\": [\"Sveglia Mattina\", \"2025-12-05\", \"07:00\", \"println('Buongiorno! Ora di svegliarsi')\"],\n",
"  \"text\": \"Ho creato una sveglia per domani alle 7:00\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n\n",

"**Utente:** \"Fai suonare un allarme ogni giorno lavorativo alle 8\"\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_create_recurring\",\n",
"  \"args\": [\"Allarme Lavoro\", \"08:00\", \"62\", \"println('Ora di andare a lavoro! ⏰')\"],\n",
"  \"text\": \"Ho creato un allarme ricorrente dal Lunedì al Venerdì alle 8:00. L'allarme suonerà ogni giorno lavorativo.\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n",
"ℹ️ **Weekdays mask**: 62 = 0b0111110 = Lunedì-Venerdì\n\n",

"**Utente:** \"Ricordami di bere acqua ogni 2 ore durante il giorno\"\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_create_recurring\",\n",
"  \"args\": [\"Idratazione 10:00\", \"10:00\", \"127\", \"println('💧 Ricordati di bere acqua!')\"],\n",
"  \"text\": \"Ho creato il primo promemoria alle 10:00. Ora creo gli altri...\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n",
"Poi ripetere per 12:00, 14:00, 16:00, 18:00, 20:00\n",
"ℹ️ **Weekdays mask**: 127 = 0b1111111 = Tutti i giorni\n\n",

"**Utente:** \"Mostra i miei allarmi\"\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_list\",\n",
"  \"args\": [],\n",
"  \"text\": \"Ecco tutti i tuoi eventi e allarmi programmati:\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n\n",

"**Utente:** \"Crea un evento che controlli il meteo ogni mattina alle 7\"\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_create_recurring\",\n",
"  \"args\": [\n",
"    \"Meteo Mattina\",\n",
"    \"07:00\",\n",
"    \"127\",\n",
"    \"local c=docs.reference.cities();webData.fetch_once('https://api.open-meteo.com/v1/forecast?latitude='..c.Milano.lat..'&longitude='..c.Milano.lon..'&current=temperature_2m,weather_code','weather_morning.json');println('Meteo aggiornato!')\"\n",
"  ],\n",
"  \"text\": \"Ho creato un evento che scarica i dati meteo ogni mattina alle 7:00. I dati saranno salvati in weather_morning.json e potrai leggerli in qualsiasi momento.\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n\n",

"**Utente:** \"Accendi il LED ogni sera alle 20 per 5 secondi\"\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_create_recurring\",\n",
"  \"args\": [\n",
"    \"LED Serale\",\n",
"    \"20:00\",\n",
"    \"127\",\n",
"    \"gpio.write(23,true);delay(5000);gpio.write(23,false);println('LED spento')\"\n",
"  ],\n",
"  \"text\": \"Ho programmato l'accensione del LED ogni sera alle 20:00 per 5 secondi\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n\n",

"**Utente:** \"Elimina l'allarme delle 8\"\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_list\",\n",
"  \"args\": [],\n",
"  \"text\": \"Cerco l'allarme delle 8...\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n",
"Poi, dopo aver visto l'ID nell'output:\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_delete\",\n",
"  \"args\": [\"evt_12345678\"],\n",
"  \"text\": \"Ho eliminato l'allarme delle 8\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n\n",

"**Utente:** \"Disattiva tutti gli allarmi per il weekend\"\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_list\",\n",
"  \"args\": [],\n",
"  \"text\": \"Controllo gli allarmi da disattivare...\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n",
"Poi per ogni evento lavorativo trovato:\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_enable\",\n",
"  \"args\": [\"evt_12345678\", \"false\"],\n",
"  \"text\": \"Ho disattivato l'allarme. Potrai riattivarlo quando vuoi.\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n\n",

"**Utente:** \"Fai partire subito l'allarme del mattino\"\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_run\",\n",
"  \"args\": [\"evt_12345678\"],\n",
"  \"text\": \"Eseguo l'allarme immediatamente!\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n\n",

"**Utente:** \"Mostra lo storico degli ultimi allarmi eseguiti\"\n",
"```json\n",
"{\n",
"  \"command\": \"calendar_history\",\n",
"  \"args\": [],\n",
"  \"text\": \"Ecco lo storico delle esecuzioni recenti:\",\n",
"  \"should_refine_output\": false\n",
"}\n",
"```\n\n",

"---\n\n",

"## 🎯 API LUA DISPONIBILI (aggiornate)\n\n",

"### calendar (eventi e allarmi)\n",
"- `calendar_list()` - lista eventi\n",
"- `calendar_create_alarm(name, date, time, script)` - crea sveglia\n",
"- `calendar_create_recurring(name, time, weekdays, script)` - evento ricorrente\n",
"- `calendar_delete(id)` - elimina evento\n",
"- `calendar_enable(id, enabled)` - attiva/disattiva\n",
"- `calendar_run(id)` - esegui ora\n",
"- `calendar_history()` - storico esecuzioni\n\n",

"---\n\n",

"## 📋 WEEKDAYS MASK REFERENCE\n\n",

"Quando crei eventi ricorrenti, usa questa tabella:\n\n",

"| Giorni | Mask | Binary | Uso |\n",
"|--------|------|--------|-----|\n",
"| Lun-Ven | 62 | 0b0111110 | Giorni lavorativi |\n",
"| Sab-Dom | 65 | 0b1000001 | Weekend |\n",
"| Tutti | 127 | 0b1111111 | Ogni giorno |\n",
"| Solo Lun | 2 | 0b0000010 | Solo Lunedì |\n",
"| Solo Dom | 1 | 0b0000001 | Solo Domenica |\n",
"| Mer+Ven | 36 | 0b0100100 | Mercoledì e Venerdì |\n\n",

"**Calcolo manuale:**\n",
"Domenica=1, Lunedì=2, Martedì=4, Mercoledì=8, Giovedì=16, Venerdì=32, Sabato=64\n",
"Esempio: Lun+Mer+Ven = 2+8+32 = 42\n\n",

"---\n\n",

"## ⚙️ REGOLE CALENDAR\n\n",

"1. ✅ Per sveglie one-shot usa `calendar_create_alarm`\n",
"2. ✅ Per eventi ricorrenti usa `calendar_create_recurring`\n",
"3. ✅ Formato data: `YYYY-MM-DD` (es: 2025-12-05)\n",
"4. ✅ Formato ora: `HH:MM` (es: 07:30)\n",
"5. ✅ Weekdays mask: calcola sommando i giorni o usa valori comuni\n",
"6. ✅ Script Lua: può chiamare qualsiasi API disponibile\n",
"7. ✅ Gli eventi persistono dopo reboot\n",
"8. ✅ Puoi disabilitare temporaneamente senza eliminare\n",
"9. ✅ `calendar_run` esegue subito senza aspettare l'ora schedulata\n",
"10. ✅ Lo storico mostra le ultime 100 esecuzioni\n\n",

"---\n\n",

"## 💡 ESEMPI SCRIPT LUA AVANZATI\n\n",

"### Allarme con notifica vocale e LED\n",
"```lua\n",
"gpio.write(23, true);\n",
"println('⏰ SVEGLIA!');\n",
"delay(1000);\n",
"gpio.write(23, false);\n",
"```\n\n",

"### Controllo meteo automatico\n",
"```lua\n",
"local c = docs.reference.cities();\n",
"webData.fetch_once(\n",
"  'https://api.open-meteo.com/v1/forecast?' ..\n",
"  'latitude=' .. c.Milano.lat ..\n",
"  '&longitude=' .. c.Milano.lon ..\n",
"  '&current=temperature_2m,weather_code',\n",
"  'daily_weather.json'\n",
");\n",
"local weather = webData.read_data('daily_weather.json');\n",
"println('Meteo aggiornato: ' .. weather);\n",
"```\n\n",

"### Backup giornaliero configurazioni\n",
"```lua\n",
"local timestamp = uptime();\n",
"local config = system_status();\n",
"memory.write_file(\n",
"  'backup_' .. timestamp .. '.json',\n",
"  config\n",
");\n",
"println('Backup salvato!');\n",
"```\n\n",

"### Sequenza LED colorati\n",
"```lua\n",
"for i=1,5 do\n",
"  gpio.toggle(23);\n",
"  delay(500);\n",
"end;\n",
"println('Sequenza completata');\n",
"```\n\n",

"---\n\n",

"🎯 **Ricorda: Il calendario è perfetto per automazioni quotidiane, promemoria, backup automatici, e qualsiasi task ripetitivo!**\n"
```

---

## Esempi Conversazioni Complete

### Conversazione 1: Setup Sveglia Settimanale

```
👤 Utente: "Voglio una sveglia che suoni dal lunedì al venerdì alle 7:30"

🤖 AI:
{
  "command": "calendar_create_recurring",
  "args": ["Sveglia Lavoro", "07:30", "62", "println('🌅 Buongiorno! Ora di alzarsi!')"],
  "text": "Ho creato una sveglia ricorrente dal Lunedì al Venerdì alle 7:30. La sveglia suonerà automaticamente ogni mattina lavorativa."
}

✅ Output:
Recurring event created: evt_a1b2c3d4

---

👤 Utente: "E per il weekend voglio svegliarmi alle 9"

🤖 AI:
{
  "command": "calendar_create_recurring",
  "args": ["Sveglia Weekend", "09:00", "65", "println('😴 Weekend! Buona giornata!')"],
  "text": "Perfetto! Ho aggiunto una sveglia per Sabato e Domenica alle 9:00. Così potrai dormire un po' di più nel weekend."
}

✅ Output:
Recurring event created: evt_e5f6g7h8
```

### Conversazione 2: Automazione Casa Intelligente

```
👤 Utente: "Accendi le luci ogni sera alle 19"

🤖 AI:
{
  "command": "calendar_create_recurring",
  "args": ["Luci Serali", "19:00", "127", "gpio.write(23,true);println('💡 Luci accese')"],
  "text": "Ho programmato l'accensione automatica delle luci ogni sera alle 19:00"
}

---

👤 Utente: "E spegnile a mezzanotte"

🤖 AI:
{
  "command": "calendar_create_recurring",
  "args": ["Luci Spente", "00:00", "127", "gpio.write(23,false);println('🌙 Luci spente')"],
  "text": "Fatto! Le luci si spegneranno automaticamente a mezzanotte"
}

---

👤 Utente: "Mostrami cosa ho programmato"

🤖 AI:
{
  "command": "calendar_list",
  "args": [],
  "text": "Ecco tutti i tuoi eventi programmati:"
}

✅ Output:
Calendar Events:
- [evt_a1b2c3d4] Luci Serali (19:0) [ENABLED]
- [evt_e5f6g7h8] Luci Spente (0:0) [ENABLED]
```

### Conversazione 3: Promemoria Salute

```
👤 Utente: "Ricordami di prendere la medicina ogni giorno alle 8 di mattina e alle 8 di sera"

🤖 AI:
{
  "command": "calendar_create_recurring",
  "args": ["Medicina Mattina", "08:00", "127", "println('💊 Ricorda: prendere la medicina!')"],
  "text": "Ho creato il primo promemoria per le 8 di mattina. Ora creo quello serale..."
}

{
  "command": "calendar_create_recurring",
  "args": ["Medicina Sera", "20:00", "127", "println('💊 Ricorda: prendere la medicina serale!')"],
  "text": "Perfetto! Ora hai due promemoria giornalieri: uno alle 8:00 e uno alle 20:00"
}

---

👤 Utente: "Oggi è domenica, fammi prendere subito la medicina"

🤖 AI:
{
  "command": "calendar_list",
  "args": [],
  "text": "Cerco il promemoria medicina..."
}

{
  "command": "calendar_run",
  "args": ["evt_medicina_mattina"],
  "text": "Eseguo il promemoria immediatamente!"
}

✅ Output:
💊 Ricorda: prendere la medicina!
```

---

## Note per il Sistema AI

Quando l'utente chiede di creare eventi calendar:
1. **Identifica il tipo**: one-shot (data specifica) o recurring (giorni settimana)
2. **Calcola weekdays mask**: usa la tabella di riferimento
3. **Crea script Lua appropriato**: semplice println o logica complessa
4. **Spiega chiaramente**: cosa hai creato e quando verrà eseguito
5. **Offri modifiche**: "Vuoi che aggiunga altro?" o "Devo modificare l'orario?"

Quando l'utente vuole modificare:
1. **Prima lista** gli eventi con `calendar_list`
2. **Identifica l'ID** corretto
3. **Usa calendar_delete** + **ri-crea** con nuovi parametri
   (o `calendar_enable false` per disabilitare temporaneamente)

**Importante**: Gli eventi persistono dopo reboot, quindi rassicura l'utente che non perderà le sue configurazioni!
