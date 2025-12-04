
std::string VoiceAssistant::getSystemPrompt() const {
    const SettingsSnapshot& settings = SettingsManager::getInstance().getSnapshot();
    std::string prompt_template = settings.voiceAssistantSystemPromptTemplate.empty()
        ? VOICE_ASSISTANT_PROMPT_TEMPLATE
        : settings.voiceAssistantSystemPromptTemplate;

    auto commands = CommandCenter::getInstance().listCommands();
    std::string command_list;
    for (size_t i = 0; i < commands.size(); ++i) {
        if (i > 0) {
            command_list += "; ";
        }
        command_list += commands[i].name;
        if (!commands[i].description.empty()) {
            command_list += " (" + commands[i].description + ")";
        }
    }

    if (command_list.empty()) {
        command_list = "none";
    }

    std::string host_list;
    BleHidManager& ble = BleHidManager::getInstance();
    if (!ble.isInitialized()) {
        host_list = "unavailable (BLE not initialized)";
    } else {
        auto bonded_peers = ble.getBondedPeers();
        if (bonded_peers.empty()) {
            host_list = "none";
        } else {
            for (size_t i = 0; i < bonded_peers.size(); ++i) {
                if (i > 0) {
                    host_list += ", ";
                }
                host_list += bonded_peers[i].address.toString();
                host_list += bonded_peers[i].isConnected ? " (connected)" : " (not connected)";
            }
        }
    }

    std::string prompt = prompt_template;
    const std::string placeholder = VOICE_ASSISTANT_COMMAND_LIST_PLACEHOLDER;
    const size_t pos = prompt.find(placeholder);
    if (pos != std::string::npos) {
        prompt.replace(pos, placeholder.length(), command_list);
    } else {
        prompt += " Available commands: " + command_list;
    }

    const std::string hosts_placeholder = VOICE_ASSISTANT_BLE_HOSTS_PLACEHOLDER;
    const size_t hosts_pos = prompt.find(hosts_placeholder);
    if (hosts_pos != std::string::npos) {
        prompt.replace(hosts_pos, hosts_placeholder.length(), host_list);
    } else {
        prompt += " Bonded BLE hosts: " + host_list + ".";
    }

    // Add Lua scripting capabilities
    prompt += "\n\nInoltre puoi utilizzare script Lua per operazioni complesse combinando GPIO, BLE, audio, web data e altre funzioni. ";
    prompt += "Per eseguire uno script Lua, usa il comando 'lua_script' con lo script come argomento. ";
    prompt += "API Lua disponibili:\n";
    prompt += "- GPIO: gpio.write(pin, value), gpio.read(pin), gpio.toggle(pin)\n";
    prompt += "- BLE: ble.type(host_mac, text), ble.send_key(host_mac, keycode, modifier), ble.mouse_move(host_mac, dx, dy), ble.click(host_mac, buttons)\n";
    prompt += "- Audio: audio.volume_up(), audio.volume_down()\n";
    prompt += "- Display: display.brightness_up(), display.brightness_down()\n";
    prompt += "- LED: led.set_brightness(percentage)\n";
    prompt += "- WebData: webData.fetch_once(url, filename), webData.fetch_scheduled(url, filename, minutes), webData.read_data(filename), webData.list_files()\n";
    prompt += "- Memory: memory.read_file(filename), memory.write_file(filename, data), memory.list_files(), memory.delete_file(filename)\n";
    prompt += "  (l'accesso ai file è limitato alle directory whitelist configurate; usa preferibilmente `/webdata` o `/memory` e lascia che venga eseguito `StorageAccessManager`)\n";
    prompt += "- Sistema: system.ping(), system.uptime(), system.heap(), system.sd_status(), system.status()\n";
    prompt += "- Timing: delay(ms)\n";
    prompt += "\n**API METEO CONSIGLIATA PER L'ITALIA:**\n";
    prompt += "Usa Open-Meteo API (gratuita, nessuna API key richiesta):\n";
    prompt += "URL: https://api.open-meteo.com/v1/forecast?latitude=LAT&longitude=LON&current=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,weather_code,wind_speed_10m&timezone=Europe/Rome\n\n";
    prompt += "Coordinate città italiane principali:\n";
    prompt += "- Milano: lat=45.4642, lon=9.1900\n";
    prompt += "- Roma: lat=41.9028, lon=12.4964\n";
    prompt += "- Napoli: lat=40.8518, lon=14.2681\n";
    prompt += "- Torino: lat=45.0703, lon=7.6869\n";
    prompt += "- Firenze: lat=43.7696, lon=11.2558\n";
    prompt += "- Venezia: lat=45.4408, lon=12.3155\n";
    prompt += "- Bologna: lat=44.4949, lon=11.3426\n";
    prompt += "- Genova: lat=44.4056, lon=8.9463\n";
    prompt += "- Palermo: lat=38.1157, lon=13.3615\n";
    prompt += "- Bari: lat=41.1171, lon=16.8719\n";
    prompt += "- Pisa: lat=43.7228, lon=10.4017\n";
    prompt += "- Livorno: lat=43.5485, lon=10.3106\n";
    prompt += "- Piombino: lat=42.9242, lon=10.5267\n\n";
    prompt += "**Response JSON contiene:**\n";
    prompt += "- current.temperature_2m: temperatura in °C\n";
    prompt += "- current.relative_humidity_2m: umidità in %\n";
    prompt += "- current.apparent_temperature: temperatura percepita in °C\n";
    prompt += "- current.wind_speed_10m: velocità vento in km/h\n";
    prompt += "- current.precipitation: precipitazioni in mm\n";
    prompt += "- current.weather_code: codice meteo (0=sereno, 1-3=nuvoloso, 61-65=pioggia, 71-77=neve)\n\n";
    prompt += "Esempi:\n";
    prompt += "- Meteo Milano: {\"command\": \"lua_script\", \"args\": [\"webData.fetch_once('https://api.open-meteo.com/v1/forecast?latitude=45.4642&longitude=9.1900&current=temperature_2m,relative_humidity_2m,wind_speed_10m&timezone=Europe/Rome', 'weather_milano.json')\"], \"text\": \"Sto scaricando i dati meteo di Milano\", \"should_refine_output\": true}\n";
    prompt += "- Leggi meteo: {\"command\": \"lua_script\", \"args\": [\"println(webData.read_data('weather_milano.json'))\"], \"text\": \"Ecco il meteo\", \"should_refine_output\": true}\n";
    prompt += "- LED lampeggiante: {\"command\": \"lua_script\", \"args\": [\"gpio.write(23, true); delay(1000); gpio.write(23, false)\"], \"text\": \"LED lampeggiato\"}\n";
    prompt += "- Scrivi testo BLE: {\"command\": \"lua_script\", \"args\": [\"ble.type('AA:BB:CC:DD:EE:FF', 'Ciao mondo!')\"], \"text\": \"Testo inviato via BLE\"}\n";
    prompt += "- Sequenza complessa: {\"command\": \"lua_script\", \"args\": [\"gpio.write(23, true); ble.type('AA:BB:CC:DD:EE:FF', 'LED acceso'); delay(2000); gpio.write(23, false); audio.volume_up()\"], \"text\": \"Sequenza completata\"}";

    // Phase 2: Smart Output Decision - Add should_refine_output field instructions
    prompt += "\n\n**NUOVO FORMATO RISPOSTA JSON (Phase 2):**\n";
    prompt += "La tua risposta JSON deve includere questi campi:\n";
    prompt += "```json\n";
    prompt += "{\n";
    prompt += "  \"command\": \"comando_da_eseguire\",\n";
    prompt += "  \"args\": [\"arg1\", \"arg2\"],\n";
    prompt += "  \"text\": \"Risposta testuale user-friendly\",\n";
    prompt += "  \"should_refine_output\": true/false,\n";
    prompt += "  \"refinement_extract\": \"text\"  // Opzionale: quale campo estrarre dal refinement\n";
    prompt += "}\n```\n\n";
    prompt += "**Campo should_refine_output:**\n";
    prompt += "Imposta questo campo a `true` se il comando produrrà output tecnico (JSON, log, dati grezzi) ";
    prompt += "che deve essere riformattato in modo user-friendly. Imposta a `false` se l'output è già comprensibile.\n\n";
    prompt += "**Campo refinement_extract (opzionale):**\n";
    prompt += "Specifica quale campo estrarre dal response di refinement. Valori possibili:\n";
    prompt += "- \"text\" (default): Estrae solo il testo user-friendly dal campo 'text' del JSON\n";
    prompt += "- \"full\" o \"json\": Restituisce l'intero JSON response per manipolazione dati strutturati\n\n";
    prompt += "**Quando usare should_refine_output=true:**\n";
    prompt += "- Comandi webData.fetch_once() o webData.read_data() (producono JSON)\n";
    prompt += "- Comandi memory.read_file() (potrebbero contenere dati grezzi)\n";
    prompt += "- Comandi heap, system_status, log_tail (producono statistiche tecniche)\n";
    prompt += "- Qualsiasi comando che produce output >200 caratteri o JSON\n\n";
    prompt += "**Quando usare should_refine_output=false:**\n";
    prompt += "- Comandi semplici come volume_up, brightness_down (output già chiaro)\n";
    prompt += "- Comandi conversazionali che non producono output tecnico\n";
    prompt += "- Se hai già formattato l'output nel campo 'text'\n\n";
    prompt += "**Esempi:**\n";
    prompt += "Query meteo per utente:\n";
    prompt += "{\"command\": \"lua_script\", \"args\": [\"webData.read_data('weather.json')\"], \"text\": \"Sto leggendo i dati meteo\", \"should_refine_output\": true, \"refinement_extract\": \"text\"}\n";
    prompt += "→ Output: \"A Milano ci sono 9.5°C...\" (solo testo user-friendly)\n\n";
    prompt += "Query meteo per elaborazione:\n";
    prompt += "{\"command\": \"lua_script\", \"args\": [\"webData.read_data('weather.json')\"], \"text\": \"Sto leggendo i dati meteo\", \"should_refine_output\": true, \"refinement_extract\": \"full\"}\n";
    prompt += "→ Output: {\"command\":\"none\",\"text\":\"...\",\"data\":{...}} (JSON completo per elaborazione)\n\n";
    prompt += "Volume up (no refinement):\n";
    prompt += "{\"command\": \"volume_up\", \"args\": [], \"text\": \"Volume aumentato\", \"should_refine_output\": false}";

    return prompt;
}
