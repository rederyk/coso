# Freenove ESP32-S3 Display (FNK0104) – Hardware Interfaces

Questa scheda esiste in due varianti:

| Codice | Touch | Note |
|--------|-------|------|
| **FNK0104A** | ❌ | Solo display SPI (tutorial `Tutorial_No_Touch`). |
| **FNK0104B** | ✅ | Display SPI + pannello capacitivo FT6336U (tutorial `Tutorial_With_Touch`). |

Il repository ora è configurato per **FNK0104B**. Se si usa la variante A rimuovere/togliere il codice touch per evitare reset I²C.

## MCU e risorse principali

- **ESP32-S3** dual-core @240 MHz con 16 MB flash QIO e 8 MB PSRAM (config `platformio.ini`).
- Bus principali esposti dalla scheda: SPI per display, I²C per touch/codec, I²S per audio, SDMMC 4-bit, ADC per batteria, GPIO per WS2812/KEY/BLE LED.

## Display SPI (ILI9341 2.8" 240×320 IPS)

Configurazione allineata al file `include/Freenove_TFT_Config.h`.

| Segnale | Pin ESP32-S3 | Note |
|---------|--------------|------|
| MOSI    | GPIO11       | Bus SPI HS (HSPI). |
| MISO    | GPIO13       | Lettura non usata dal driver ma cablata. |
| SCLK    | GPIO12       | Clock SPI 40 MHz. |
| CS      | GPIO10       | `TFT_CS`. |
| DC/RS   | GPIO46       | `TFT_DC`. |
| RST     | GPIO48 / EN  | `TFT_RST` è cablato a reset di sistema (definito `-1`). |
| BL      | GPIO45       | Backlight (HIGH = acceso). |

Uso: libreria `TFT_eSPI` + LVGL. Ricordarsi di chiamare `enableBacklight()` e inizializzare `TFT_eSPI` prima di LVGL.

## Touch capacitivo FT6336U

Fonte: `Tutorial_With_Touch/Sketch_11_Touch.ino`.

| Segnale | Pin ESP32-S3 | Descrizione |
|---------|--------------|-------------|
| I²C SDA | GPIO16       | Bus dedicato al touch. |
| I²C SCL | GPIO15       | Clock I²C. |
| RST     | GPIO18       | Reset attivo basso. |
| INT     | GPIO17       | Interrupt (non necessario per polling LVGL). |
| Alimentazione | 3V3 | Consumo < 5 mA. |

- Indirizzo I²C fisso **0x38**.
- Coordinate “grezze” 0–239 (X) / 0–319 (Y); per rotazione landscape serve `swapXY=1` e `invertY=1`.
- Per debug usare `findTouchController()` in `src/utils/i2c_scanner.cpp` con i pin sopra.

## Audio: Codec ES8311 + Amplificatore FM8002E

Pin estratti da `public.h` e sketch 07/17:

| Segnale | Pin ESP32-S3 | Funzione |
|---------|--------------|----------|
| I2S_MCK | GPIO4        | Master clock verso ES8311. |
| I2S_BCK | GPIO5        | Bit clock. |
| I2S_WS  | GPIO7        | Word select (LRCK). |
| I2S_DOUT| GPIO8        | Dati da MCU → codec → amplificatore/speaker. |
| I2S_DIN | GPIO6        | Dati da codec (microfono MEMS esterno). |
| I²C SDA | GPIO16       | Condiviso con FT6336U (bus 400 kHz). |
| I²C SCL | GPIO15       | Come sopra. |

Componenti:
- **ES8311** codec I²S slave (gestisce DAC per speaker + ADC per microfono electret).
- **FM8002E** amplificatore mono collegato all’uscita del codec.
- **Microfono MEMS LMA2718B** cablato all’ingresso analogico dell’ES8311.

Suggerimenti:
1. Inizializzare l’I²C prima del codec.
2. Settare il bus I²S con `setPins(I2S_BCK, I2S_WS, I2S_DOUT, I2S_DIN, I2S_MCK)` e sample-rate 44.1 kHz (musica) o 16 kHz (echo).
3. Gestire il guadagno microfono tramite `es8311_microphone_gain_set`.

## SD Card (slot microSD, modalità SDMMC 4-bit)

Pin confermati da `public.h` e `Sketch_06.1_SDMMC_Test`.

| Segnale | Pin ESP32-S3 |
|---------|--------------|
| CMD     | GPIO40 |
| CLK     | GPIO38 |
| D0      | GPIO39 |
| D1      | GPIO41 |
| D2      | GPIO48 |
| D3      | GPIO47 |

- Alimentazione 3V3, livello logico nativo.
- Inizializzare con `SD_MMC.begin("/sdcard", false, false, BOARD_MAX_SDMMC_FREQ, 5)`.
- Se serve la modalità 1-bit usare solo D0 e lasciare D1-D3 flottanti.

## WS2812 RGB LED

- LED singolo indirizzabile collegato a **GPIO42** (`LEDS_PIN`).
- Usare librerie tipo `Freenove_WS2812` o `Adafruit_NeoPixel`, 5V tolerant grazie a transistor sulla board.
- Ricordarsi di alimentare con 5 V quando brightness elevata.

## Pulsanti e GPIO utente

| Pin | Funzione | Note |
|-----|----------|------|
| GPIO0 | `KEY` frontale | Input con pull-up interno. Usato anche come BOOT/DFU. |
| EN / Reset | Pulsante dedicato | Resetta MCU e display. |

Altri GPIO liberi/documentati nei tutorial:

| Pin | Uso tipico | Commento |
|-----|------------|----------|
| GPIO1/2 | Port I²C esterno | Disponibili sul connettore a pettine (“I/O pin port”). |
| GPIO14/21 | Opzioni alternative I²C (vedi `findTouchController`). |
| GPIO9 | **BAT_ADC** (divider 1:1). `analogReadMilliVolts` × 2 = tensione batteria. |
| GPIO45 | Backlight (già usato). |

## Alimentazione e batteria

- Connettore JST 2 pin per LiPo 3.7 V, caricato dal **TP4054** (max 500 mA).
- Monitor batteria: `GPIO9` via partitore (moltiplicare per 2).
- LDO principale: **ME6217** (3.3 V, 500 mA). Evitare di superare 800 mA su 3.3 V rail.

## Porte esterne

- **USB-C**: programmazione + alimentazione 5 V + USB-CDC/MSC.
- **UART header**: TX0/RX0 disponibili sulla fila di pin (vedi datasheet ESP32-S3).
- **I²C port**: disponibile su connettore dedicato (default SDA=1, SCL=2 se non si utilizza il bus interno 15/16).
- **Speaker port**: connettore JST 2 pin (0.5 W/8 Ω consigliato).
- **Microfono MEMS** integrato (nessun pin richiesto).

## Checklist per nuovi progetti

1. **Seleziona la variante giusta**: per FNK0104B mantieni `touch_driver.h` così com’è; per FNK0104A disabilita `touch_driver_init()` per liberare I²C 15/16.
2. **Display/TFT**: assicurati che `Freenove_TFT_Config.h` sia incluso con `-include` (già configurato in `platformio.ini`).
3. **Touch**: dopo aver flashato un nuovo firmware, verifica il log Serial – deve comparire `FT6336 detected`. In caso contrario eseguire `findTouchController()`.
4. **Audio**: inizializza l’I²C a 400 kHz, poi il codec (ES8311) e infine l’I²S driver. Ricordare `Audio::setPinout()` con i pin sopra.
5. **SD**: prima di montare FS, chiamare `SD_MMC.begin` e verificare `CardType`.
6. **WS2812/KEY**: usare `pinMode(KEY_PIN, INPUT_PULLUP)` e debounce; per il LED usare `strip.begin()` e `strip.show()`.
7. **Batteria**: sfrutta `analogReadMilliVolts(9) * 2 / 1000.0f`.

Tenendo questa tabella sott’occhio si evita di prendere i pin della variante “no touch” e si rende più semplice portare il firmware su altri progetti Freenove/ESP32-S3.
