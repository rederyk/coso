local function trim(text)
    if not text then
        return nil
    end
    text = text:gsub("^%s+", ""):gsub("%s+$", "")
    if text == "" then
        return nil
    end
    return text
end

local function limit(text, max_len)
    if not text then
        return nil
    end
    if #text > max_len then
        return text:sub(1, max_len) .. "..."
    end
    return text
end

local function summarize_weather()
    local raw = webData.read_data('weather.json')
    if not raw or raw == '' then
        return "Meteo: nessun cache salvato (weather.json mancante)."
    end

    local temperature = raw:match('"temperature_2m"%s*:%s*([-%d%.]+)')
        or raw:match('"temperature"%s*:%s*([-%d%.]+)')
    local weather_code = raw:match('"weather_code"%s*:%s*(%d+)')
        or raw:match('"weathercode"%s*:%s*(%d+)')
    local wind = raw:match('"wind_speed_10m"%s*:%s*([-%d%.]+)')
        or raw:match('"windspeed"%s*:%s*([-%d%.]+)')

    local summary = "Meteo cache: "
    if temperature then
        summary = summary .. temperature .. "C"
    else
        summary = summary .. "temperatura n/d"
    end
    if weather_code then
        summary = summary .. ", codice " .. weather_code
    end
    if wind then
        summary = summary .. ", vento " .. wind .. " m/s"
    end

    return summary
end

local function summarize_notes()
    local note = trim(memory.read_file('assistant_note.md'))
    if not note then
        return "Note utente: nessuna preferenza salvata."
    end
    note = limit(note, 260)
    return "Note utente:\n" .. note
end

local function summarize_files()
    local ok, files = pcall(memory.list_files)
    if not ok or not files or #files == 0 then
        return "File memoria: directory vuota."
    end

    local lines = {}
    local max_display = 6
    for i = 1, math.min(#files, max_display) do
        lines[#lines + 1] = "- " .. tostring(files[i])
    end
    if #files > max_display then
        lines[#lines + 1] = string.format("... (%d file extra)", #files - max_display)
    end

    return "File memoria:\n" .. table.concat(lines, "\n")
end

local function tts_rules()
    return table.concat({
        "TTS rapido:",
        "- usa tts.speak(text) solo su richiesta esplicita o alert critici",
        "- restituisci sempre anche testo per fallback",
        "- memorizza il path MP3 ritornato se serve riascoltarlo"
    }, "\n")
end

local sections = {
    summarize_weather(),
    summarize_notes(),
    summarize_files(),
    tts_rules()
}

return table.concat(sections, "\n\n")
