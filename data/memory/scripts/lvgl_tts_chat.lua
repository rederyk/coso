-- lvgl_tts_chat.lua: Dettatura automatica per screen AI LVGL
-- Usa: speak_and_play("testo_risposta_ai")

function speak_and_play(response_text)
    if response_text == "" then
        println("Nessun testo da dettare")
        return false
    end

    println("LVGL AI: Generazione TTS per '" .. response_text .. "'")

    local file_path = tts.speak(response_text)  -- Crea WAV in /sd/audio/
    if not file_path then
        println("Errore TTS - Mantieni testo LVGL")
        return false
    end

    println("TTS generato: " .. file_path)

    -- Volume per chat (non disturbare)
    radio.set_volume(60)

    -- Play
    local ok, msg = radio.play(file_path)
    if ok then
        println("Dettatura in corso: " .. file_path)

        -- Opzionale: Poll per fine (screen status)
        local attempts = 0
        while attempts < 60 do  -- Max 60s
            delay(1000)
            local _, status = radio.status()
            if string.find(status, "ENDED") or string.find(status, "STOPPED") then
                break
            end
            attempts = attempts + 1
        end

        -- Cleanup
        memory.delete_file(file_path)
        println("Dettatura completata")
        return true
    else
        println("Errore play: " .. msg)
        -- Cleanup su errore
        memory.delete_file(file_path)
        return false
    end
end

-- Esempio hook per screen (simula call dopo risposta)
-- speak_and_play("Il meteo è soleggiato a Roma.")
