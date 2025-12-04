# 🤖 AI Guide to Device Memory & Usage

This README guides the AI (embedded in the voice assistant) on using the `/memory` directory for persistent storage and interacting with device features via Lua scripts. Access everything through the sandbox: `memory.*` for files, `docs.*` for references, APIs for hardware/control. Focus on generating safe `lua_script` commands in JSON responses.

## 📁 Memory Files & AI Usage

- **`user_profile.md`**: Store user prefs (e.g., voice sensitivity, BLE devices). Read: `memory.read_file('user_profile.md')`; Write: `memory.write_file('user_profile.md', 'data')` to personalize (e.g., "Preferred city: Milano").
- **`project_context.md`**: Track device state/context (e.g., active apps, last command). Update after tasks: Append "2025-04-12: Fetched weather for Piombino".
- **`task_history.md`**: Log completed actions (e.g., "GPIO toggle on pin 23"). AI appends successes: `local history = memory.read_file('task_history.md') .. '\\nCompleted: ' .. task; memory.write_file('task_history.md', history)`.
- **`code_snippets.md`**: Save useful Lua patterns (e.g., weather fetch). AI uses for generation: Read to recall "Standard GPIO: gpio.write(23, true)".
- **`conversation_notes.md`**: Key interactions (e.g., "User likes Italian weather queries"). Read for context in responses.

**Access Pattern**: Use 2-step: 1) Consult `docs.*` if needed, 2) Read/write via `memory.*`. Permissions: Read/write OK in `/memory`; `/docs` read-only. Limit to ~20KB total.

## 🛠️ How AI Uses Device Features

The AI generates JSON with `"command": "lua_script", "args": [script]` to execute on-device. Use `println()` for output (captured/refined).

### Core Lua APIs
- **Hardware Control**: `gpio.write(pin, value)`, `ble.type(mac, text)`, `audio.volume_up()`, `display.brightness_down()`, `led.set_brightness(50)`. Consult `docs.api.gpio()` etc. for args.
- **Data Fetch**: `webData.fetch_once('url', 'file.json')`; Then `webData.read_data('file.json')` for JSON (refine output: `"should_refine_output": true`).
- **System**: `system.status()`, `system.heap()` for monitoring. Use for diagnostics.
- **Docs Access**: `println(docs.reference.cities())` for coords (JSON); `docs.examples.weather_query()` for templates.

### Response Workflow
1. **Parse User Query**: E.g., "Tempo a Roma?" → Check history/profile for prefs.
2. **Consult if Needed**: If unknown, first script: `"args": ["println(docs.reference.cities())"]` to get lat/lon.
3. **Generate Script**: E.g., `local roma = docs.reference.cities().Roma; webData.fetch_once('https://api.open-meteo.com/v1/forecast?latitude=' .. roma.lat .. '&longitude=' .. roma.lon .. '&current=temperature_2m', 'weather.json'); println(webData.read_data('weather.json'))`.
4. **Refine**: Set `"should_refine_output": true` for user-friendly text (e.g., "A Roma 20°C soleggiato").
5. **Update Memory**: Post-execution, append to `task_history.md` or `conversation_notes.md` via script.

### Tips for AI
- **Safety**: Sandbox blocks dangerous ops (no `os.execute`). Always check args/docs.
- **Efficiency**: Cache in conversation (e.g., remember coords after first fetch). Pre-load common docs if query matches (e.g., "meteo" → inject cities).
- **Errors**: If fetch fails, fallback: `"text": "Impossibile connettersi, prova dopo."`; Log to notes.
- **Examples**:
  - **GPIO**: User: "Accendi LED". Script: `gpio.write(23, true); println('LED acceso')`. Update history.
  - **BLE**: User: "Digita hello su PC". Script: `ble.type('AA:BB:CC:DD:EE:FF', 'Hello')`.
  - **Weather**: As above, refine JSON to natural Italian response.

Use memory to maintain state (e.g., ongoing tasks). Store only essentials; query device APIs directly for real-time data. This enables seamless, persistent device control.
