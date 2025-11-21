#pragma once

#include <lvgl.h>
#include <cstdint>

/**
 * Utility per calcolare automaticamente il colore del testo
 * basato sul colore di sfondo per massimizzare la leggibilità.
 *
 * Utilizza la formula della luminosità relativa W3C WCAG 2.0
 * per determinare se il testo debba essere chiaro o scuro.
 */
class ColorUtils {
public:
    /**
     * Inverte un colore RGB per massimizzare il contrasto.
     * Ogni componente RGB viene invertito: nuovo_valore = 255 - vecchio_valore
     *
     * @param color Colore da invertire in formato lv_color_t
     * @return lv_color_t - Colore invertito
     */
    static lv_color_t invertColor(lv_color_t color) {
        // Estrai i componenti RGB dal colore LVGL
        uint8_t r = LV_COLOR_GET_R(color);
        uint8_t g = LV_COLOR_GET_G(color);
        uint8_t b = LV_COLOR_GET_B(color);

        // Converti RGB565 a RGB888 se necessario
        #if LV_COLOR_DEPTH == 16
            r = (r * 255) / 31;  // 5 bit -> 8 bit
            g = (g * 255) / 63;  // 6 bit -> 8 bit
            b = (b * 255) / 31;  // 5 bit -> 8 bit
        #endif

        // Inverti ogni componente
        uint8_t inv_r = 255 - r;
        uint8_t inv_g = 255 - g;
        uint8_t inv_b = 255 - b;

        // Crea e ritorna il colore invertito
        return lv_color_make(inv_r, inv_g, inv_b);
    }

    /**
     * Versione alternativa che accetta un colore in formato hex uint32_t
     *
     * @param color_hex Colore in formato esadecimale (0xRRGGBB)
     * @return lv_color_t - Colore invertito
     */
    static lv_color_t invertColor(uint32_t color_hex) {
        return invertColor(lv_color_hex(color_hex));
    }

    /**
     * Calcola il colore del testo ottimale (bianco o nero) in base al colore di sfondo.
     *
     * @param bg_color Colore di sfondo in formato lv_color_t
     * @return lv_color_t - Bianco (0xFFFFFF) se lo sfondo è scuro, Nero (0x000000) se è chiaro
     */
    static lv_color_t getContrastingTextColor(lv_color_t bg_color) {
        // Estrai i componenti RGB dal colore LVGL
        uint8_t r = LV_COLOR_GET_R(bg_color);
        uint8_t g = LV_COLOR_GET_G(bg_color);
        uint8_t b = LV_COLOR_GET_B(bg_color);

        // Converti RGB565 a RGB888 se necessario
        // LVGL usa RGB565, quindi dobbiamo espandere i valori
        #if LV_COLOR_DEPTH == 16
            r = (r * 255) / 31;  // 5 bit -> 8 bit
            g = (g * 255) / 63;  // 6 bit -> 8 bit
            b = (b * 255) / 31;  // 5 bit -> 8 bit
        #endif

        // Calcola la luminosità relativa usando la formula W3C WCAG 2.0
        // https://www.w3.org/TR/WCAG20-TECHS/G17.html
        float luminance = calculateRelativeLuminance(r, g, b);

        // Se la luminosità è > 0.5, lo sfondo è chiaro, usa testo scuro
        // Altrimenti usa testo chiaro
        return (luminance > 0.5f) ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);
    }

    /**
     * Versione alternativa che accetta un colore in formato hex uint32_t
     *
     * @param bg_color_hex Colore di sfondo in formato esadecimale (0xRRGGBB)
     * @return lv_color_t - Bianco o Nero per massimo contrasto
     */
    static lv_color_t getContrastingTextColor(uint32_t bg_color_hex) {
        return getContrastingTextColor(lv_color_hex(bg_color_hex));
    }

    /**
     * Versione che restituisce un colore con opacity personalizzata.
     * Utile per testi secondari o hint.
     *
     * @param bg_color Colore di sfondo
     * @param opacity_dark Opacità per testo scuro (0-255), default 230 (~90%)
     * @param opacity_light Opacità per testo chiaro (0-255), default 230 (~90%)
     * @return lv_color_t con opacità suggerita (da applicare separatamente)
     */
    static lv_color_t getContrastingTextColorWithHint(lv_color_t bg_color,
                                                       lv_opa_t* out_opacity = nullptr,
                                                       lv_opa_t opacity_dark = 230,
                                                       lv_opa_t opacity_light = 230) {
        uint8_t r = LV_COLOR_GET_R(bg_color);
        uint8_t g = LV_COLOR_GET_G(bg_color);
        uint8_t b = LV_COLOR_GET_B(bg_color);

        #if LV_COLOR_DEPTH == 16
            r = (r * 255) / 31;
            g = (g * 255) / 63;
            b = (b * 255) / 31;
        #endif

        float luminance = calculateRelativeLuminance(r, g, b);
        bool use_dark_text = (luminance > 0.5f);

        if (out_opacity) {
            *out_opacity = use_dark_text ? opacity_dark : opacity_light;
        }

        return use_dark_text ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);
    }

    /**
     * Restituisce un colore muted (attenuato) per testi secondari.
     * Calcola automaticamente un colore grigio chiaro o scuro in base allo sfondo.
     *
     * @param bg_color Colore di sfondo
     * @return lv_color_t - Grigio chiaro o scuro ottimale
     */
    static lv_color_t getMutedTextColor(lv_color_t bg_color) {
        uint8_t r = LV_COLOR_GET_R(bg_color);
        uint8_t g = LV_COLOR_GET_G(bg_color);
        uint8_t b = LV_COLOR_GET_B(bg_color);

        #if LV_COLOR_DEPTH == 16
            r = (r * 255) / 31;
            g = (g * 255) / 63;
            b = (b * 255) / 31;
        #endif

        float luminance = calculateRelativeLuminance(r, g, b);

        // Per sfondi chiari usa grigio scuro (~60%), per sfondi scuri usa grigio chiaro (~70%)
        return (luminance > 0.5f) ? lv_color_hex(0x606060) : lv_color_hex(0xB0B0B0);
    }

    /**
     * Restituisce un colore muted per testi secondari (versione hex)
     */
    static lv_color_t getMutedTextColor(uint32_t bg_color_hex) {
        return getMutedTextColor(lv_color_hex(bg_color_hex));
    }

    /**
     * Applica automaticamente il colore del testo a tutti i label figli di un oggetto
     * in base al colore di sfondo dell'oggetto stesso.
     *
     * @param obj Oggetto parent di cui aggiornare i label figli
     */
    static void applyAutoTextColor(lv_obj_t* obj) {
        if (!obj) return;

        // Ottieni il colore di sfondo dell'oggetto
        lv_color_t bg_color = lv_obj_get_style_bg_color(obj, 0);
        lv_color_t text_color = getContrastingTextColor(bg_color);

        // Aggiorna il colore del testo dell'oggetto stesso se è un label
        if (lv_obj_check_type(obj, &lv_label_class)) {
            lv_obj_set_style_text_color(obj, text_color, 0);
        }

        // Aggiorna ricorsivamente tutti i figli
        uint32_t child_count = lv_obj_get_child_cnt(obj);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t* child = lv_obj_get_child(obj, i);
            if (child && lv_obj_check_type(child, &lv_label_class)) {
                lv_obj_set_style_text_color(child, text_color, 0);
            }
        }
    }

    /**
     * Applica il colore del testo a un bottone e al suo label interno
     * in base al colore di sfondo del bottone.
     *
     * @param btn Bottone da aggiornare
     */
    static void applyAutoButtonTextColor(lv_obj_t* btn) {
        if (!btn) return;

        lv_color_t bg_color = lv_obj_get_style_bg_color(btn, 0);
        lv_color_t text_color = getContrastingTextColor(bg_color);

        // Trova e aggiorna tutti i label figli del bottone
        uint32_t child_count = lv_obj_get_child_cnt(btn);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t* child = lv_obj_get_child(btn, i);
            if (child && lv_obj_check_type(child, &lv_label_class)) {
                lv_obj_set_style_text_color(child, text_color, 0);
            }
        }
    }

    /**
     * Calcola il contrast ratio tra due colori secondo WCAG 2.0
     *
     * @param color1 Primo colore
     * @param color2 Secondo colore
     * @return float - Contrast ratio (1.0 - 21.0)
     */
    static float getContrastRatio(lv_color_t color1, lv_color_t color2) {
        uint8_t r1 = LV_COLOR_GET_R(color1);
        uint8_t g1 = LV_COLOR_GET_G(color1);
        uint8_t b1 = LV_COLOR_GET_B(color1);

        uint8_t r2 = LV_COLOR_GET_R(color2);
        uint8_t g2 = LV_COLOR_GET_G(color2);
        uint8_t b2 = LV_COLOR_GET_B(color2);

        #if LV_COLOR_DEPTH == 16
            r1 = (r1 * 255) / 31;
            g1 = (g1 * 255) / 63;
            b1 = (b1 * 255) / 31;

            r2 = (r2 * 255) / 31;
            g2 = (g2 * 255) / 63;
            b2 = (b2 * 255) / 31;
        #endif

        float lum1 = calculateRelativeLuminance(r1, g1, b1);
        float lum2 = calculateRelativeLuminance(r2, g2, b2);

        float lighter = (lum1 > lum2) ? lum1 : lum2;
        float darker = (lum1 > lum2) ? lum2 : lum1;

        return (lighter + 0.05f) / (darker + 0.05f);
    }

private:
    /**
     * Calcola la luminosità relativa secondo W3C WCAG 2.0
     *
     * @param r Componente rosso (0-255)
     * @param g Componente verde (0-255)
     * @param b Componente blu (0-255)
     * @return float - Luminosità relativa (0.0 - 1.0)
     */
    static float calculateRelativeLuminance(uint8_t r, uint8_t g, uint8_t b) {
        // Normalizza RGB a 0.0 - 1.0
        float rf = r / 255.0f;
        float gf = g / 255.0f;
        float bf = b / 255.0f;

        // Applica la correzione gamma
        rf = (rf <= 0.03928f) ? (rf / 12.92f) : powf((rf + 0.055f) / 1.055f, 2.4f);
        gf = (gf <= 0.03928f) ? (gf / 12.92f) : powf((gf + 0.055f) / 1.055f, 2.4f);
        bf = (bf <= 0.03928f) ? (bf / 12.92f) : powf((bf + 0.055f) / 1.055f, 2.4f);

        // Calcola la luminosità relativa con i pesi W3C
        return 0.2126f * rf + 0.7152f * gf + 0.0722f * bf;
    }
};
