# WebDataManager Implementation Roadmap

## Overview
Implementare un WebDataManager programmabile per ESP32-S3 che permetta di scaricare dati web regolarmente e renderli disponibili agli script Lua del voice assistant in modo controllato.

## Architettura Finale
- **WebDataManager**: Core manager per download e caching
- **HTTP Client**: Implementazione richieste HTTP in uscita
- **Security Layer**: Whitelist domini e controlli sicurezza
- **Lua Integration**: API per script Lua del voice assistant
- **Weather Widget**: Widget dashboard che usa i dati cachati

## Fasi di Implementazione

### Fase 1: Core Infrastructure (1-2 sessioni)
**Obiettivo**: Creare l'infrastruttura base del WebDataManager

#### Task 1.1: Creare WebDataManager Class ✅ COMPLETATO
- [x] Creare `include/core/web_data_manager.h`
- [x] Implementare `src/core/web_data_manager.cpp`
- [x] Aggiungere metodi base: `fetchOnce()`, `readData()`, `listFiles()`
- [x] Integrare con StorageManager per caching su LittleFS

#### Task 1.2: HTTP Client Implementation ✅ COMPLETATO
- [x] Implementare HTTP GET requests con WiFiClientSecure
- [x] Gestire HTTPS con certificati (setInsecure per development)
- [x] Timeout e retry logic
- [x] Error handling e logging

#### Task 1.3: Integrazione Sistema ✅ COMPLETATO
- [x] Registrare WebDataManager nel main.cpp
- [x] Aggiungere dipendenze in platformio.ini (HTTPClient, WiFiClientSecure)
- [x] Test build senza errori ✅

### Fase 2: Security & Scheduling (1-2 sessioni)
**Obiettivo**: Aggiungere controlli sicurezza e schedulazione

#### Task 2.1: Domain Whitelist System ✅ COMPLETATO
- [x] Implementare whitelist domini configurabili via settings.json
- [x] Validazione URL prima delle richieste
- [x] Configurazione tramite settings.json con `loadAllowedDomainsFromSettings()`
- [x] Persistence impostazioni su reboot

#### Task 2.2: Rate Limiting & Size Limits ✅ COMPLETATO
- [x] Max richieste per ora per dominio (10 default)
- [x] Max dimensione file cachati configurabile (50KB default)
- [x] Automatic cleanup vecchi file con `cleanupOldFiles()`
- [x] Enhanced logging per rate limiting

#### Task 2.3: Scheduled Downloads ✅ COMPLETATO
- [x] Sistema di schedulazione con LVGL timers
- [x] Metodo `fetchScheduled()` con intervalli configurabili
- [x] Persistence schedule state su reboot con JSON su LittleFS
- [x] Improved error handling nei timer callbacks

### Fase 3: Lua Integration (1 sessione)
**Obiettivo**: Esporre API ai script Lua del voice assistant

#### Task 3.1: Lua Bindings ✅ COMPLETATO
- [x] Aggiungere funzioni Lua nel LuaSandbox:
  - `webData.fetch_once(url, filename)` - Download dati una tantum
  - `webData.fetch_scheduled(url, filename, minutes)` - Schedulare download periodici
  - `webData.read_data(filename)` - Lettura dati dal file system
  - `webData.list_files()` - Lista file cachati
- [x] Registrare funzioni C native nel setupSandbox Lua
- [x] Implementare bridge thread-safe con WebDataManager singleton
- [x] Gestire errori appropriati e valori di ritorno Lua

#### Task 3.2: Lua API Testing ✅ COMPLETATO
- [x] Test integrato nel sistema voice assistant
- [x] Verifica accessibilità API tramite comandi vocali
- [x] Test error handling per URL non validi e file assenti
- [x] Aggiornamento system prompt con esempi pratici di webData API

### Fase 4: Weather Widget (1 sessione) ✅ COMPLETATO
**Obiettivo**: Creare widget meteo che usa WebDataManager

#### Task 4.1: Weather Widget Implementation ✅ COMPLETATO
- [x] Creare `src/widgets/weather_widget.h` e `.cpp`
- [x] Implementare parsing JSON meteo semplice con cJSON
- [x] Display temperatura, condizioni meteo, icone emoji
- [x] Gestire stati: loading, error, no data con messaggi appropriati
- [x] Refresh automatico ogni 30 minuti
- [x] Utilizzo WebDataManager per download da Open-Meteo API
- [x] Weather codes Open-Meteo supportati (0-99)

#### Task 4.2: Dashboard Integration ✅ COMPLETATO
- [x] Aggiungere WeatherWidget al DashboardScreen
- [x] Layout responsive con altri widget (system, clock)
- [x] Tema e styling consistente con background blue
- [x] Aggiornamento roadmap API in headers

### Fase 5: Testing & Refinement (1-2 sessioni)
**Obiettivo**: Testing completo e ottimizzazioni

#### Task 5.1: Unit Testing
- [ ] Test HTTP requests con mock server
- [ ] Test caching e schedulazione
- [ ] Test sicurezza whitelist

#### Task 5.2: Integration Testing
- [ ] Test end-to-end con API reali (Open-Meteo)
- [ ] Test voice assistant integration
- [ ] Performance testing e memory usage

#### Task 5.3: Documentation & Examples
- [ ] Documentare API Lua
- [ ] Esempi script per meteo, news, etc.
- [ ] Guida configurazione whitelist

## API Reference Finale

### C++ API
```cpp
class WebDataManager {
public:
    // Download
    bool fetchOnce(const std::string& url, const std::string& filename);
    bool fetchScheduled(const std::string& url, const std::string& filename, uint32_t minutes);

    // Data Access
    std::string readData(const std::string& filename);
    std::vector<std::string> listFiles();

    // Security
    void addAllowedDomain(const std::string& domain);
    void removeAllowedDomain(const std::string& domain);

    // Management
    void cancelScheduled(const std::string& filename);
    void cleanupOldFiles(uint32_t max_age_hours = 24);
};
```

### Lua API
```lua
-- Download una tantum
webData.fetch_once("https://api.example.com/data", "data.json")

-- Download schedulato (ogni 60 minuti)
webData.fetch_scheduled("https://api.example.com/data", "data.json", 60)

-- Lettura dati
local data = webData.read_data("data.json")

-- Lista file
local files = webData.list_files()
```

## Configurazione

### settings.json
```json
{
  "webData": {
    "enabled": true,
    "maxFileSize": 51200,
    "maxRequestsPerHour": 10,
    "allowedDomains": [
      "api.open-meteo.com",
      "newsapi.org",
      "api.openweathermap.org"
    ]
  }
}
```

## Sessioni Suggeste
1. **Sessione 1**: Task 1.1 + 1.2 (Core WebDataManager) ✅ COMPLETATO
2. **Sessione 2**: Task 1.3 + 2.1 + 2.2 + 2.3 (Security & Scheduling Enhancement) ✅ COMPLETATO
3. **Sessione 3**: Task 3.1 + 3.2 (Lua Integration completa)
4. **Sessione 4**: Task 4.1 + 4.2 (Weather Widget) ✅ COMPLETATO
5. **Sessione 5**: Task 5.1 + 5.2 + 5.3 (Testing completo + Docs)

## Rischi e Mitigazioni
- **Rate limiting API esterne**: Implementare exponential backoff
- **Memoria limitata ESP32**: File size limits e cleanup automatico
- **Connettività instabile**: Retry logic e timeout appropriati
- **Sicurezza**: Strict whitelist e input validation

## Testing Checklist
- [ ] HTTP requests a domini allowed
- [ ] HTTP requests bloccati per domini non allowed
- [ ] Caching funziona correttamente
- [ ] Schedulazione persiste su reboot
- [ ] Lua API accessibile dal voice assistant
- [ ] Weather widget display corretto
- [ ] Memory usage sotto limiti ESP32
- [ ] Error handling robusto
