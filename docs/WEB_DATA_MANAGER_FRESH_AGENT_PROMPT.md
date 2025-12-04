# WebDataManager Implementation - Fresh Agent Prompt

## 🎯 MISSION OVERVIEW
Implementare un sistema di WebDataManager programmabile per ESP32-S3 che permetta al voice assistant di accedere a dati web in modo controllato e sicuro.

## 📋 CURRENT STATUS (Sessione 1 Completata)

### ✅ COMPLETATO nella Sessione 1:
- **WebDataManager Class**: Header (`include/core/web_data_manager.h`) e implementation (`src/core/web_data_manager.cpp`) completi
- **HTTP Client**: Implementazione con WiFiClientSecure per richieste HTTPS
- **Sicurezza Base**: Whitelist domini, rate limiting (10 req/ora per dominio), size limits (50KB)
- **Caching**: Storage su LittleFS nella cartella `/webdata/`
- **Scheduling**: Sistema base con LVGL timers per download programmati
- **Integrazione Sistema**: Registrato nel `main.cpp`, compila senza errori
- **Thread Safety**: Mutex FreeRTOS per concorrenza

### 🔄 STATO ATTUALE DEL CODICE:
- **Compilazione**: ✅ OK (build completato con successo)
- **Inizializzazione**: ✅ WebDataManager si avvia automaticamente nel setup()
- **API C++**: ✅ Disponibili metodi `fetchOnce()`, `readData()`, `listFiles()`, etc.
- **Storage**: ✅ Directory `/webdata/` creata automaticamente

## 🚀 PROSSIME SESSIONI DA COMPLETARE

### **Sessione 2: Security & Scheduling Enhancement**
**Focus**: Rafforzare sicurezza e completare il sistema di schedulazione

1. **Domain Whitelist System**:
   - Configurazione whitelist tramite `settings.json`
   - Aggiungere metodo `loadAllowedDomainsFromSettings()`
   - Persistenza impostazioni su reboot

2. **Rate Limiting & Size Limits**:
   - Implementare `checkRateLimit()` e `updateRateLimit()` (struttura base già presente)
   - Automatic cleanup vecchi file con `cleanupOldFiles()`
   - Logging migliorato per debugging

3. **Scheduled Downloads**:
   - Persistence dello stato schedulazione su reboot
   - Migliorare gestione errori nei timer callback
   - Aggiungere metodi per gestione schedulazioni (`cancelScheduled()`, `getScheduledTasks()`)

### **Sessione 3: Lua Integration**
**Focus**: Esporre WebDataManager agli script Lua del voice assistant

1. **Lua Bindings nel VoiceAssistant**:
   - Modificare `VoiceAssistant::LuaSandbox::setupSandbox()` in `src/core/voice_assistant.cpp`
   - Aggiungere funzioni C per Lua:
     ```cpp
     static int lua_webdata_fetch_once(lua_State* L);
     static int lua_webdata_fetch_scheduled(lua_State* L);
     static int lua_webdata_read_data(lua_State* L);
     static int lua_webdata_list_files(lua_State* L);
     ```

2. **Lua API**:
   ```lua
   -- Esempi di utilizzo negli script del voice assistant
   webData.fetch_once("https://api.open-meteo.com/v1/forecast?latitude=45.46&longitude=9.19", "weather.json")
   webData.fetch_scheduled("https://newsapi.org/v2/top-headlines", "news.json", 60)
   local weather = webData.read_data("weather.json")
   local files = webData.list_files()
   ```

### **Sessione 4: Weather Widget**
**Focus**: Creare widget dashboard che usa i dati del WebDataManager

1. **Weather Widget** (`src/widgets/weather_widget.h` + `src/widgets/weather_widget.cpp`):
   - Parsing JSON semplice per dati meteo
   - Display: temperatura, condizioni, umidità
   - Stati: loading, error, no data

2. **Dashboard Integration**:
   - Aggiungere WeatherWidget al `DashboardScreen`
   - Layout responsive
   - Tema e styling consistente

### **Sessione 5: Testing & Documentation**
**Focus**: Testing completo e documentazione finale

## 🛠️ ARCHITETTURA TECNICA

### **Classi Principali**:
```cpp
class WebDataManager {
    // Download methods
    RequestResult fetchOnce(const std::string& url, const std::string& filename);
    bool fetchScheduled(const std::string& url, const std::string& filename, uint32_t minutes);

    // Data access
    std::string readData(const std::string& filename);

    // Security
    void addAllowedDomain(const std::string& domain);
    bool isDomainAllowed(const std::string& url) const;
};
```

### **Integrazione Sistema**:
- **Main.cpp**: WebDataManager inizializzato in `setup()`
- **VoiceAssistant**: Lua bindings in `LuaSandbox::setupSandbox()`
- **Storage**: Usa LittleFS, cartella `/webdata/`
- **Timers**: LVGL timers per schedulazione

### **Dipendenze**:
- HTTPClient (già incluso in platformio)
- WiFiClientSecure (già incluso)
- LVGL (per timers)
- FreeRTOS (per mutex)

## 📋 TESTING CHECKLIST

### **Unit Tests**:
- [ ] HTTP requests a domini allowed funzionano
- [ ] HTTP requests a domini non allowed bloccati
- [ ] Caching su file system funziona
- [ ] Rate limiting applicato correttamente
- [ ] Scheduled downloads funzionano

### **Integration Tests**:
- [ ] Lua API accessibile dal voice assistant
- [ ] Weather widget display corretto
- [ ] Memory usage sotto limiti ESP32
- [ ] Error handling robusto

## 🎯 PRIORITÀ IMMEDIATE

1. **Iniziare Sessione 2**: Completare sicurezza e schedulazione
2. **Test Intermedi**: Verificare che tutto compili dopo ogni modifica
3. **Documentazione**: Aggiornare roadmap con progressi
4. **Error Handling**: Migliorare logging e gestione errori

## 🔍 DEBUGGING TIPS

- **Logs**: Usa `ESP_LOGI(TAG, "message")` per debug
- **Serial Monitor**: Verifica inizializzazione WebDataManager
- **File System**: Controlla cartella `/webdata/` via web interface
- **Memory**: Monitora RAM usage con `logMemoryStats()`

## 📁 FILE IMPORTANTI DA CONOSCERE

- `include/core/web_data_manager.h` - API pubblica
- `src/core/web_data_manager.cpp` - Implementazione core
- `src/core/voice_assistant.cpp` - Per Lua integration
- `src/main.cpp` - Inizializzazione sistema
- `docs/WEB_DATA_MANAGER_ROADMAP.md` - Guida completa

## 🚦 STATUS CHECK

**Prima di iniziare:**
- [ ] Codice compila senza errori
- [ ] WebDataManager si inizializza correttamente
- [ ] Directory `/webdata/` viene creata
- [ ] Settings.json configurato correttamente

**Dopo ogni sessione:**
- [ ] Build successful
- [ ] Roadmap aggiornata
- [ ] Nuove funzionalità testate
- [ ] Documentazione aggiornata

---

**Ready to continue with Session 2? Let's build the most secure and powerful web data system for ESP32! 🚀**
