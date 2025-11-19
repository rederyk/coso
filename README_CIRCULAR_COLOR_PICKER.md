# Circular Color Picker

Widget LVGL per la selezione di colori tramite un cerchio cromatico HSV.

## Funzionamento

Il widget mostra un cerchio dove:
- **Centro**: Colori desaturati (grigio/nero)
- **Bordo**: Colori saturi e vividi
- **Angolo**: Tonalità (Hue 0-360°)
- **Distanza dal centro**: Saturazione (0-100%)
- **Brightness**: Luminosità fissa, modificabile tramite API

Un cursore circolare bianco con ombra indica la posizione del colore selezionato.

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

- **Tap/Click**: Sposta il cursore nella posizione toccata
- **Drag**: Sposta il cursore seguendo il dito
- **Auto scroll block**: Blocca automaticamente lo scroll dei container padre durante il drag

## Implementazione

### Struttura Interna

```cpp
struct PickerData {
    lv_obj_t* canvas;        // Canvas per il cerchio di colori
    lv_obj_t* cursor;        // Cursore indicatore
    lv_coord_t size;         // Diametro del picker
    uint16_t hue;            // Tonalità corrente (0-359)
    uint8_t saturation;      // Saturazione corrente (0-100)
    uint8_t brightness;      // Luminosità corrente (0-100)
    bool dragging;           // Stato di trascinamento
};
```

### Rendering

Il cerchio viene disegnato pixel-per-pixel su un canvas LVGL:

1. Canvas pulito con trasparenza
2. Per ogni pixel dentro il raggio:
   - Calcola angolo → Hue
   - Calcola distanza dal centro → Saturazione
   - Converte HSV → RGB
   - Disegna il pixel

```cpp
void draw_color_circle(lv_obj_t* canvas, lv_coord_t size, uint8_t brightness) {
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            if (pixel_inside_circle) {
                hue = angle_from_center;
                saturation = distance_from_center;
                color = lv_color_hsv_to_rgb(hue, saturation, brightness);
                lv_canvas_set_px_color(canvas, x, y, color);
            }
        }
    }
}
```

### Cursore

- Dimensione: 14x14 pixel
- Bordo bianco opaco (3px, 90% opacità)
- Centro semi-trasparente nero (30% opacità)
- Ombra nera sfumata (8px, 50% opacità, offset Y+2)

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
- Il brightness NON viene modificato da `set_rgb()` o `set_hsv()`
- Il widget gestisce automaticamente la conversione RGB ↔ HSV
- La posizione del cursore viene calcolata automaticamente da hue/saturation
