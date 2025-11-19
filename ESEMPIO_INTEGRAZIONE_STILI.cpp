/**
 * ESEMPIO: Come aggiungere un selettore di stile per il Color Picker
 *
 * Questo esempio mostra come aggiungere 3 pulsanti per cambiare lo stile
 * dei color picker in theme_settings_screen.cpp
 */

// 1. Aggiungi variabili membro alla classe ThemeSettingsScreen (nel file .h)
/*
private:
    lv_obj_t* style_buttons[3] = {nullptr};
*/

// 2. Aggiungi questo codice nel metodo build() di theme_settings_screen.cpp,
//    subito PRIMA della sezione "Color picker 2D"

void ThemeSettingsScreen::build(lv_obj_t* parent) {
    // ... codice esistente ...

    // ========== SEZIONE STILI COLOR PICKER ==========
    lv_obj_t* style_card = create_card(content, "Stile Color Picker");
    lv_obj_set_height(style_card, LV_SIZE_CONTENT);

    lv_obj_t* style_hint = lv_label_create(style_card);
    lv_label_set_text(style_hint, "Scegli l'aspetto delle ruote colore:");
    lv_obj_set_style_text_color(style_hint, lv_color_hex(0x9fb0c8), 0);
    lv_obj_set_style_text_font(style_hint, &lv_font_montserrat_12, 0);

    // Container pulsanti stile
    lv_obj_t* style_container = lv_obj_create(style_card);
    lv_obj_remove_style_all(style_container);
    lv_obj_set_size(style_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(style_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(style_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(style_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(style_container, 8, 0);

    // Struttura per i pulsanti stile
    struct StyleButton {
        const char* label;
        const char* emoji;
        CircularColorPicker::Style style;
    };

    StyleButton styles[] = {
        {"Modern", "✨", CircularColorPicker::Style::MODERN},
        {"Pixel", "🕹️", CircularColorPicker::Style::PIXEL},
        {"Glass", "💎", CircularColorPicker::Style::GLASS}
    };

    for (int i = 0; i < 3; i++) {
        lv_obj_t* btn = lv_btn_create(style_container);
        lv_obj_set_size(btn, 80, 50);
        lv_obj_set_style_radius(btn, 12, 0);

        // Stile default: Modern è selezionato
        if (i == 0) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x5df4ff), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_30, 0);
            lv_obj_set_style_border_width(btn, 2, 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(0x5df4ff), 0);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a2332), 0);
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(0x3a4a5c), 0);
        }

        // Layout verticale per emoji + testo
        lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(btn, 2, 0);

        // Emoji
        lv_obj_t* emoji = lv_label_create(btn);
        lv_label_set_text(emoji, styles[i].emoji);
        lv_obj_set_style_text_font(emoji, &lv_font_montserrat_16, 0);

        // Label
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, styles[i].label);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xf0f0f0), 0);

        // Salva riferimento al pulsante
        style_buttons[i] = btn;

        // Event handler
        lv_obj_add_event_cb(btn, handleStyleButton, LV_EVENT_CLICKED,
                           (void*)(uintptr_t)i);
    }

    // ... resto del codice (color pickers, etc.) ...
}

// 3. Aggiungi questo handler statico in theme_settings_screen.cpp

void ThemeSettingsScreen::handleStyleButton(lv_event_t* e) {
    auto* screen = static_cast<ThemeSettingsScreen*>(
        lv_event_get_user_data(lv_event_get_current_target(e))
    );
    int style_index = (int)(uintptr_t)lv_event_get_user_data(e);

    // Mappa indice -> stile
    CircularColorPicker::Style styles[] = {
        CircularColorPicker::Style::MODERN,
        CircularColorPicker::Style::PIXEL,
        CircularColorPicker::Style::GLASS
    };

    // Applica il nuovo stile a tutti i color picker
    if (screen->primary_wheel) {
        CircularColorPicker::set_style(screen->primary_wheel, styles[style_index]);
    }
    if (screen->accent_wheel) {
        CircularColorPicker::set_style(screen->accent_wheel, styles[style_index]);
    }
    if (screen->card_wheel) {
        CircularColorPicker::set_style(screen->card_wheel, styles[style_index]);
    }
    if (screen->dock_wheel) {
        CircularColorPicker::set_style(screen->dock_wheel, styles[style_index]);
    }

    // Aggiorna lo stato visivo dei pulsanti
    for (int i = 0; i < 3; i++) {
        if (i == style_index) {
            // Pulsante selezionato
            lv_obj_set_style_bg_color(screen->style_buttons[i],
                                     lv_color_hex(0x5df4ff), 0);
            lv_obj_set_style_bg_opa(screen->style_buttons[i], LV_OPA_30, 0);
            lv_obj_set_style_border_width(screen->style_buttons[i], 2, 0);
            lv_obj_set_style_border_color(screen->style_buttons[i],
                                         lv_color_hex(0x5df4ff), 0);
        } else {
            // Pulsante non selezionato
            lv_obj_set_style_bg_color(screen->style_buttons[i],
                                     lv_color_hex(0x1a2332), 0);
            lv_obj_set_style_bg_opa(screen->style_buttons[i], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(screen->style_buttons[i], 1, 0);
            lv_obj_set_style_border_color(screen->style_buttons[i],
                                         lv_color_hex(0x3a4a5c), 0);
        }
    }

    Serial.printf("🎨 Style changed to: %d\n", style_index);
}

// 4. Aggiungi la dichiarazione del metodo in theme_settings_screen.h
/*
private:
    static void handleStyleButton(lv_event_t* e);
*/

/**
 * RISULTATO:
 *
 * Avrai 3 pulsanti in alto:
 * - ✨ Modern (default, flat con shadow)
 * - 🕹️ Pixel (retro con griglia)
 * - 💎 Glass (glassmorphism)
 *
 * Cliccando su uno dei pulsanti, tutti i 4 color picker cambieranno
 * istantaneamente stile con un ridisegno automatico.
 */
