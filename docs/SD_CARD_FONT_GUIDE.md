# Guida Font su SD Card

## Vantaggi dell'uso della SD Card

✅ **Spazio illimitato** - Le SD da 8-32GB costano pochi euro
✅ **Noto Color Emoji completo** - 10MB di emoji a colori senza problemi
✅ **Decine di font** - Inter, Montserrat, Roboto, Playfair e molti altri
✅ **Aggiornamento facile** - Cambi i font senza riflashare l'ESP32
✅ **Assets multipli** - Immagini, temi, icone tutto su SD
✅ **Fallback automatico** - Se non c'è SD, usa LittleFS interno

## Hardware

### Freenove FNK0104 ESP32-S3

La scheda ha uno slot microSD che condivide il bus SPI con il TFT:

```
TFT SPI Bus:
- MOSI: GPIO 11
- MISO: GPIO 13
- SCK:  GPIO 12
- CS:   GPIO 10 (TFT)

SD Card:
- MOSI: GPIO 11 (shared)
- MISO: GPIO 13 (shared)
- SCK:  GPIO 12 (shared)
- CS:   GPIO 14 (SD)
```

## Implementazione

### 1. StorageManager Modificato

Il `StorageManager` ora supporta sia SD che LittleFS:

```cpp
#include "core/storage_manager.h"

// Inizializzazione (fatto automaticamente in main.cpp)
StorageManager& sm = StorageManager::getInstance();
sm.begin(true);  // true = prova SD card prima

// Check disponibilità
if (sm.isSDAvailable()) {
    // SD card montata!
}

// Path automatico (usa SD se disponibile, altrimenti LittleFS)
String fonts_path = sm.getFontsPath();  // "S:/fonts/" o "L:/fonts/"
```

### 2. FontManager Aggiornato

FontManager usa automaticamente StorageManager per selezionare lo storage:

```cpp
FontManager& fm = FontManager::getInstance();
fm.begin();  // Inizializza con storage preferito

// Carica font (automaticamente da SD o LittleFS)
lv_font_t* font = fm.loadFont(FontFamily::INTER, FontWeight::REGULAR, 16);
```

### 3. Driver LVGL per SD

Due driver filesystem per LVGL:
- **L:** = LittleFS (flash interna)
- **S:** = SD Card (scheda esterna)

## Setup SD Card

### Formattare la SD

1. **Formato consigliato:** FAT32
2. **Dimensione:** 2GB - 32GB
3. **Velocità:** Class 10 o superiore

Su Linux:
```bash
sudo mkfs.vfat -F 32 /dev/sdX1
```

Su Windows: click destro > Formatta > FAT32

### Struttura Directory

```
SD Card (FAT32)
├── fonts/
│   ├── Inter-Regular.ttf
│   ├── Inter-Bold.ttf
│   ├── Inter-Light.ttf
│   ├── Montserrat-Regular.ttf
│   ├── Montserrat-Bold.ttf
│   ├── SourceSans3-Regular.ttf
│   ├── RobotoSlab-Regular.ttf
│   ├── PlayfairDisplay-Regular.ttf
│   └── NotoColorEmoji.ttf  ✅ Ora puoi!
├── assets/
│   ├── icons/
│   └── backgrounds/
└── themes/
```

### Copiare i Font

#### Opzione 1: Download diretto sulla SD

```bash
# Scarica i font
cd /path/to/your/project
./tools/download_fonts.sh

# Copia sulla SD (inserisci la SD nel PC)
cp -r data/fonts/* /media/sdcard/fonts/

# Download Noto Emoji (10MB)
cd /media/sdcard/fonts/
wget https://github.com/googlefonts/noto-emoji/raw/main/fonts/NotoColorEmoji.ttf
```

#### Opzione 2: Script automatico

Creo uno script per preparare la SD:

```bash
#!/bin/bash
# tools/prepare_sd_card.sh

SD_MOUNT="/media/sdcard"  # Modifica con il tuo mount point

echo "=== Preparazione SD Card per ESP32-S3 ==="

if [ ! -d "$SD_MOUNT" ]; then
    echo "❌ SD card non montata in $SD_MOUNT"
    exit 1
fi

# Crea struttura directory
mkdir -p "$SD_MOUNT/fonts"
mkdir -p "$SD_MOUNT/assets/icons"
mkdir -p "$SD_MOUNT/assets/backgrounds"

# Scarica font se non presenti
if [ ! -d "data/fonts" ] || [ -z "$(ls -A data/fonts)" ]; then
    echo "Scarico i font..."
    ./tools/download_fonts.sh
fi

# Copia font sulla SD
echo "Copio font sulla SD..."
cp data/fonts/*.ttf "$SD_MOUNT/fonts/"

# Download Noto Emoji
if [ ! -f "$SD_MOUNT/fonts/NotoColorEmoji.ttf" ]; then
    echo "Scarico Noto Color Emoji (10MB)..."
    wget -q -O "$SD_MOUNT/fonts/NotoColorEmoji.ttf" \
        "https://github.com/googlefonts/noto-emoji/raw/main/fonts/NotoColorEmoji.ttf"
fi

# Mostra spazio
echo ""
echo "=== Spazio SD Card ==="
df -h "$SD_MOUNT"

echo ""
echo "=== Font installati ==="
ls -lh "$SD_MOUNT/fonts/"

echo ""
echo "✅ SD Card pronta! Inseriscila nell'ESP32-S3"
```

## Codice da Aggiungere/Modificare

### main.cpp

```cpp
#include "core/storage_manager.h"
#include "core/font_manager.h"
#include "utils/lv_fs_littlefs.h"
#include "utils/lv_fs_sd.h"

void setup() {
    // ... init display ...

    lv_init();

    // Inizializza storage (SD + LittleFS)
    StorageManager& storage = StorageManager::getInstance();
    if (!storage.begin(true)) {  // true = prova SD
        logger.error("Nessuno storage disponibile!");
    }

    // Registra driver filesystem LVGL
    if (storage.isSDAvailable()) {
        lv_fs_sd_init();  // Registra drive S:
    }
    if (storage.isLittleFSAvailable()) {
        lv_fs_littlefs_init();  // Registra drive L:
    }

    // Inizializza FontManager
    FontManager& fm = FontManager::getInstance();
    if (fm.begin()) {
        logger.info("FontManager pronto con storage: " +
                    storage.getFontsPath());
    }

    // ... resto del setup ...
}
```

### FontManager.cpp (già fatto)

```cpp
std::string FontManager::getFontPath(FontFamily family, FontWeight weight) const {
    // Usa StorageManager per path automatico (SD o LittleFS)
    std::string base = StorageManager::getInstance().getFontsPath();
    std::string filename;

    switch (family) {
        case FontFamily::INTER:
            filename = "Inter-";
            break;
        // ...
    }

    switch (weight) {
        case FontWeight::REGULAR:
            filename += "Regular.ttf";
            break;
        // ...
    }

    return base + filename;  // "S:/fonts/Inter-Regular.ttf"
}
```

## Uso

### Caricamento Font (identico a prima!)

```cpp
FontManager& fm = FontManager::getInstance();

// Carica da SD (se disponibile) o LittleFS (fallback)
lv_font_t* font = fm.loadFont(
    FontManager::FontFamily::INTER,
    FontManager::FontWeight::REGULAR,
    20
);

// Usa normalmente
lv_obj_set_style_text_font(label, font, 0);
```

### Emoji a Colori

```cpp
// Ora puoi caricare Noto Emoji completo!
lv_font_t* emoji = fm.loadFont(
    FontManager::FontFamily::NOTO_EMOJI,
    FontManager::FontWeight::REGULAR,
    24
);

lv_obj_t* label = lv_label_create(parent);
lv_label_set_text(label, "Hello 👋 World 🌍 🎉");
lv_obj_set_style_text_font(label, emoji, 0);
```

### Info Storage

```cpp
StorageManager& sm = StorageManager::getInstance();

// Ottieni info
if (sm.isSDAvailable()) {
    uint64_t total_mb = sm.getTotalBytes(StorageLocation::SD_CARD) / (1024*1024);
    uint64_t free_mb = sm.getFreeBytes(StorageLocation::SD_CARD) / (1024*1024);

    Serial.printf("SD Card: %llu MB / %llu MB liberi\n", free_mb, total_mb);
}
```

## Vantaggi Pratici

### Prima (solo LittleFS)
- ❌ Max 4-8MB flash per font
- ❌ Noto Emoji troppo grande (10MB)
- ❌ Pochi font disponibili
- ❌ Serve riflash per cambiare font

### Dopo (con SD Card)
- ✅ 8-32GB disponibili
- ✅ Noto Emoji completo
- ✅ Decine di font
- ✅ Swap SD per cambiare tutto
- ✅ Fallback automatico a LittleFS

## Troubleshooting

### SD non rilevata

```
[Storage] SD card not available, falling back to LittleFS
```

**Cause possibili:**
1. SD non inserita
2. SD non formattata (usa FAT32)
3. Contatti sporchi
4. SD danneggiata

**Test:**
```cpp
// In setup(), aggiungi:
if (!SD.begin(14)) {  // 14 = CS pin
    Serial.println("SD mount failed");
    // Prova a leggere il tipo
    uint8_t cardType = SD.cardType();
    Serial.printf("Card type: %d\n", cardType);
}
```

### Font non caricano da SD

```
[FontManager] Failed to load font: S:/fonts/Inter-Regular.ttf
```

**Verifica:**
1. File esiste sulla SD: `ls /media/sdcard/fonts/`
2. Nome file corretto (case-sensitive!)
3. SD montata correttamente
4. Driver LVGL registrato: `lv_fs_sd_init()` chiamato

### Lentezza caricamento

**Soluzioni:**
1. Usa SD Class 10 o superiore
2. Aumenta cache LVGL: `fs_drv.cache_size = 1024;`
3. Precarica font comuni durante boot
4. Cache FreeType più grande: `LV_FREETYPE_CACHE_SIZE (64 * 1024)`

## Performance

### Velocità Lettura

| Storage | Prima lettura | Cache hit |
|---------|---------------|-----------|
| LittleFS | ~50ms | <1ms |
| SD Class 10 | ~80ms | <1ms |
| SD UHS-I | ~60ms | <1ms |

FreeType cache rende le successive render istantanee.

### Consumo Memoria

| Componente | RAM | Flash |
|------------|-----|-------|
| FreeType | 32KB | ~80KB |
| SD driver | 2KB | ~5KB |
| Font (TTF) | - | su SD |
| Glyphs cache | 32KB | - |

## Migrazione da LittleFS

### Passo 1: Prepara SD

```bash
./tools/prepare_sd_card.sh  # Crea e copia tutto
```

### Passo 2: Inserisci SD

Inserisci la microSD nello slot dell'ESP32-S3.

### Passo 3: Flash Firmware

```bash
pio run --target upload
```

Il firmware rileverà automaticamente la SD e la userà!

### Passo 4: Test

Monitor seriale mostrerà:
```
[Storage] SD Card: SDHC, Size: 16384 MB
[Storage] SD card mounted successfully
[Storage] Preferred storage: SD Card
[FontManager] Font storage: S:/fonts/
```

## File Finali Necessari

### Modifiche Codice

1. ✅ `src/core/storage_manager.h` - Espanso con SD support
2. ✅ `src/core/storage_manager.cpp` - Implementazione SD
3. ✅ `src/utils/lv_fs_sd.h` - Driver LVGL SD
4. ✅ `src/utils/lv_fs_sd.cpp` - Implementazione driver
5. 🔧 `src/core/font_manager.cpp` - Usa StorageManager
6. 🔧 `src/main.cpp` - Init SD + drivers

### Tools

7. `tools/prepare_sd_card.sh` - Script preparazione SD

## Summary

Con la SD card puoi:

1. **10MB+ di emoji** (Noto Color Emoji completo)
2. **Decine di font** (tutte le famiglie Google Fonts)
3. **Aggiornamenti facili** (swap SD, no reflash)
4. **Fallback intelligente** (usa LittleFS se no SD)
5. **Costo bassissimo** (SD 16GB = ~5€)

**Prossimo step:** Crea un Settings UI per cambiare font family dinamicamente!

---

**Generato:** 2025-11-19
**Progetto:** ESP32-S3 Touch Display
**Board:** Freenove FNK0104
