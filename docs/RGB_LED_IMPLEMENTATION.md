# Implementazione RGB LED Manager

## Panoramica
È stata aggiunta una classe singleton `RgbLedManager` per gestire il LED RGB integrato del Freenove ESP32-S3 (WS2812B su GPIO 42) e notificare lo stato del dispositivo tramite indicazioni luminose.

## File Aggiunti
- `src/drivers/rgb_led_driver.h` - Header della classe RgbLedManager
- `src/drivers/rgb_led_driver.cpp` - Implementazione del driver LED RGB

## Modifiche ai File Esistenti

### Settings Manager
**File**: `src/core/settings_manager.h` e `src/core/settings_manager.cpp`
- Aggiunto parametro `ledBrightness` (0-100%) nella struttura `SettingsSnapshot`
- Aggiunta chiave `LedBrightness` nell'enum `SettingKey`
- Implementati metodi `getLedBrightness()` e `setLedBrightness()`
- Valore predefinito: 50%

### Main
**File**: `src/main.cpp`
- Inizializzazione del RgbLedManager al boot (GPIO48)
- Caricamento della luminosità LED dai settings
- Listener per aggiornare la luminosità del LED quando cambia nei settings
- Animazione di boot all'avvio (effetto arcobaleno)

### WiFi Manager
**File**: `src/core/wifi_manager.cpp`
- LED blu lampeggiante durante connessione WiFi
- LED verde fisso quando WiFi connesso
- LED rosso lampeggiante in caso di errore WiFi
- LED spento se WiFi non configurato
- Monitoraggio continuo dello stato della connessione

### BLE Manager
**File**: `src/core/ble_manager.cpp`
- LED ciano lampeggiante durante advertising BLE
- LED ciano fisso quando client BLE connesso
- LED torna ad advertising quando client disconnesso

## Stati del LED

| Stato | Colore | Comportamento | Descrizione |
|-------|--------|---------------|-------------|
| `OFF` | Spento | Fisso | LED spento |
| `BOOT` | Arcobaleno | Animato | Sequenza di avvio |
| `WIFI_CONNECTING` | Blu | Lampeggiante (500ms) | Connessione WiFi in corso |
| `WIFI_CONNECTED` | Verde | Fisso | WiFi connesso con successo |
| `WIFI_ERROR` | Rosso | Lampeggiante (500ms) | Errore connessione WiFi |
| `BLE_ADVERTISING` | Ciano | Lampeggiante (500ms) | BLE in modalità advertising |
| `BLE_CONNECTED` | Ciano | Fisso | Client BLE connesso |
| `ERROR` | Rosso | Fisso | Errore generico |
| `CUSTOM` | Personalizzato | Fisso | Colore RGB personalizzato |

## Utilizzo Programmativo

```cpp
#include "drivers/rgb_led_driver.h"

// Ottieni istanza singleton
RgbLedManager& led = RgbLedManager::getInstance();

// Inizializza (GPIO 42 di default per Freenove ESP32-S3)
led.begin(42);

// Imposta luminosità (0-100%)
led.setBrightness(50);

// Imposta stato predefinito
led.setState(RgbLedManager::LedState::WIFI_CONNECTED);

// Imposta colore personalizzato RGB (0-255)
led.setColor(255, 128, 0);  // Arancione

// Spegni LED
led.off();
```

## Integrazione con Settings

La luminosità del LED viene salvata automaticamente nei settings persistenti e può essere modificata dall'interfaccia utente. Le modifiche vengono applicate immediatamente senza riavvio.

```cpp
SettingsManager& settings = SettingsManager::getInstance();

// Leggi luminosità corrente
uint8_t brightness = settings.getLedBrightness();

// Modifica luminosità (salvataggio automatico)
settings.setLedBrightness(75);
```

## Note Tecniche

- **Driver**: RMT (Remote Control Transceiver) di Arduino ESP32 per timing preciso WS2812B
- **GPIO**: 42 (pin LED RGB integrato su Freenove ESP32-S3)
- **Task FreeRTOS**: Task dedicato per animazioni (Core 1, priorità 1)
- **Frequenza aggiornamento**: 50ms (20 Hz)
- **Controllo luminosità**: Applicato via software (0-100%)
- **Formato colore**: GRB (Green-Red-Blue) per WS2812B
- **Timing RMT**: 100ns per tick
  - Bit 0: HIGH 400ns (4 ticks), LOW 800ns (8 ticks)
  - Bit 1: HIGH 800ns (8 ticks), LOW 400ns (4 ticks)

## Conflitti WiFi/BLE

Poiché sia WiFi che BLE possono modificare lo stato del LED, viene data priorità a:
1. **WiFi** - Se WiFi è attivo, mostra lo stato WiFi
2. **BLE** - Se WiFi non è configurato, mostra lo stato BLE

La logica di priorità è gestita automaticamente dai manager.
