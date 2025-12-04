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

#### Task 1.1: Creare WebDataManager Class
- [ ] Creare `include/core/web_data_manager.h`
- [ ] Implementare `src/core/web_data_manager.cpp`
- [ ] Aggiungere metodi base: `fetchOnce()`, `readData()`, `listFiles()`
- [ ] Integrare con StorageManager per caching su LittleFS

#### Task 1.2: HTTP Client Implementation
- [ ] Implementare HTTP GET requests con WiFiClientSecure
- [ ] Gestire HTTPS con certificati
- [ ] Timeout e retry logic
- [ ] Error handling e logging

#### Task 1.3: Integrazione Sistema
- [ ] Registrare WebDataManager nel main.cpp
- [ ] Aggiungere dipendenze in platformio.ini
- [ ] Test build senza errori

### Fase 2: Security & Scheduling (1-2 sessioni)
**Obiettivo**: Aggiungere controlli sicurezza e schedulazione

#### Task 2.1: Domain Whitelist System
- [ ] Implementare whitelist domini configurabili
- [ ] Validazione URL prima delle richieste
- [ ] Configurazione tramite settings.json

#### Task 2.2: Rate Limiting & Size Limits
- [ ] Max richieste per ora per dominio
- [ ] Max dimensione file cachati (es. 50KB)
- [ ] Automatic cleanup vecchi file

#### Task 2.3: Scheduled Downloads
- [ ] Sistema di schedulazione con FreeRTOS timers
- [ ] Metodo `fetchScheduled()` con intervalli configurabili
- [ ] Persistence schedule state su reboot

### Fase 3: Lua Integration (1 sessione)
**Obiettivo**: Esporre API ai script Lua del voice assistant

#### Task 3.1: Lua Bindings
- [ ] Aggiungere funzioni Lua nel LuaSandbox:
  - `webData_fetch_once(url, filename)`
  - `webData_fetch_scheduled(url, filename, minutes)`
  - `webData_read_data(filename)`
  - `webData_list_files()`
- [ ] Registrare funzioni in `setupSandbox()`

#### Task 3.2: Lua API Testing
- [ ] Test script Lua semplice per download dati
- [ ] Verifica lettura dati cachati
- [ ] Test error handling in Lua

### Fase 4: Weather Widget (1 sessione)
**Obiettivo**: Creare widget meteo che usa WebDataManager

#### Task 4.1: Weather Widget Implementation
- [ ] Creare `src/widgets/weather_widget.h` e `.cpp`
- [ ] Implementare parsing JSON meteo semplice
- [ ] Display temperatura, condizioni, umidità
- [ ] Gestire stati: loading, error, no data

#### Task 4.2: Dashboard Integration
- [ ] Aggiungere WeatherWidget al DashboardScreen
- [ ] Layout responsive con altri widget
- [ ] Tema e styling consistente

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
1. **Sessione 1**: Task 1.1 + 1.2 (Core WebDataManager)
2. **Sessione 2**: Task 1.3 + 2.1 (Integrazione + Sicurezza base)
3. **Sessione 3**: Task 2.2 + 2.3 + 3.1 (Scheduling + Lua API)
4. **Sessione 4**: Task 3.2 + 4.1 + 4.2 (Testing Lua + Weather Widget)
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
