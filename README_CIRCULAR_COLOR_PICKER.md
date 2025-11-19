# Circular Color Picker

Widget LVGL per la selezione di colori tramite un cerchio cromatico HSV con modalità giorno/notte.

## Funzionamento

Il widget mostra un cerchio dove:
- **Angolo**: Tonalità (Hue 0-360°)
- **Distanza dal centro**: Saturazione (0-100%)

### Modalità Giorno (default)
- Cerchio HSV standard con luminosità massima
- Colori brillanti e vividi
- Centro grigio desaturato

### Modalità Notte
- Gradiente radiale da nero al centro a colori scuri al bordo
- Centro nero puro (0% brightness)
- Bordo con colori scuri (50% brightness)
- Curva esponenziale per transizione graduale
- **Double-tap** sul cerchio per alternare tra giorno/notte

## API

### Creazione

```cpp
lv_obj_t* CircularColorPicker::create(
    lv_obj_t* parent,        // Genitore LVGL
    lv_coord_t size,         // Diametro in pixel
    uint8_t brightness = 70  // Luminosità iniziale (0-100)
)
```

**Esempio:**
```cpp
lv_obj_t* picker = CircularColorPicker::create(parent, 110, 70);
```

### Getter

```cpp
lv_color_t get_rgb(lv_obj_t* obj);
lv_color_hsv_t get_hsv(lv_obj_t* obj);
uint8_t get_brightness(lv_obj_t* obj);
```

**Esempio:**
```cpp
lv_color_t rgb = CircularColorPicker::get_rgb(picker);
lv_color_hsv_t hsv = CircularColorPicker::get_hsv(picker);
uint8_t brightness = CircularColorPicker::get_brightness(picker);
```

### Setter

```cpp
void set_rgb(lv_obj_t* obj, lv_color_t color);
void set_hsv(lv_obj_t* obj, lv_color_hsv_t hsv);
void set_brightness(lv_obj_t* obj, uint8_t brightness);
```

**Esempio:**
```cpp
// Imposta colore da RGB
lv_color_t blue = lv_color_hex(0x0000FF);
CircularColorPicker::set_rgb(picker, blue);

// Imposta colore da HSV
lv_color_hsv_t cyan = {180, 100, 70};
CircularColorPicker::set_hsv(picker, cyan);

// Cambia luminosità (ridisegna il cerchio)
CircularColorPicker::set_brightness(picker, 80);
```

## Eventi

Il widget emette `LV_EVENT_VALUE_CHANGED` quando l'utente seleziona un colore.

```cpp
void on_color_change(lv_event_t* e) {
    lv_obj_t* picker = lv_event_get_target(e);
    lv_color_t color = CircularColorPicker::get_rgb(picker);

    // Usa il colore
    lv_obj_set_style_bg_color(target, color, 0);
}

lv_obj_add_event_cb(picker, on_color_change, LV_EVENT_VALUE_CHANGED, nullptr);
```

## Interazione Touch

- **Tap/Drag**: Seleziona il colore nella posizione toccata
- **Double-tap**: Alterna tra modalità giorno e notte
- **Auto scroll block**: Blocca automaticamente lo scroll durante il drag

## Dettagli Tecnici

### Modalità Notte - Curva Esponenziale
```cpp
// Brightness = 50% × (normalized_distance)^1.8
float brightness_factor = powf(normalized, 1.8f);
pixel_brightness = 50 * brightness_factor;
```

Questa curva garantisce:
- Centro nero puro (0%)
- Transizione graduale verso colori scuri
- Bordo fisso al 50%

### Cursore
- Dimensione: 14×14 px, bordo bianco (3px), centro semi-trasparente
- Ombra per profondità visiva

## Memoria

- **Buffer canvas**: Allocato in SPIRAM
- **Formato**: `LV_IMG_CF_TRUE_COLOR_ALPHA` (con trasparenza)
- **Dimensione tipica**: ~24KB per picker 110x110
- **Cleanup**: Automatico quando il widget viene distrutto

## Esempio Completo

```cpp
#include "widgets/circular_color_picker.h"

class ColorDemo {
    lv_obj_t* picker;
    lv_obj_t* preview;

    static void handle_color(lv_event_t* e) {
        ColorDemo* demo = (ColorDemo*)lv_event_get_user_data(e);
        lv_color_t color = CircularColorPicker::get_rgb(demo->picker);
        lv_obj_set_style_bg_color(demo->preview, color, 0);
    }

public:
    void build(lv_obj_t* parent) {
        lv_obj_t* container = lv_obj_create(parent);

        // Picker
        picker = CircularColorPicker::create(container, 110, 70);
        lv_obj_add_event_cb(picker, handle_color, LV_EVENT_VALUE_CHANGED, this);

        // Preview
        preview = lv_obj_create(container);
        lv_obj_set_size(preview, 200, 50);

        // Colore iniziale
        lv_color_t initial = lv_color_hex(0xFF5733);
        CircularColorPicker::set_rgb(picker, initial);
        lv_obj_set_style_bg_color(preview, initial, 0);
    }
};
```

## Note

- `set_brightness()` ridisegna l'intero cerchio (operazione costosa)
- `get_rgb()` e `get_hsv()` restituiscono il colore effettivo considerando la modalità attiva
- In modalità notte, il brightness dipende dalla posizione radiale del cursore
- Double-tap con finestra di 350ms per cambio modalità
