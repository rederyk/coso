#pragma once

#include "../core/keyboard_layout.h"

// Standard HID keycodes
constexpr uint8_t KEY_MOD_LSHIFT = 0x02;
constexpr uint8_t KEY_A = 0x04;
constexpr uint8_t KEY_B = 0x05;
constexpr uint8_t KEY_C = 0x06;
constexpr uint8_t KEY_D = 0x07;
constexpr uint8_t KEY_E = 0x08;
constexpr uint8_t KEY_F = 0x09;
constexpr uint8_t KEY_G = 0x0A;
constexpr uint8_t KEY_H = 0x0B;
constexpr uint8_t KEY_I = 0x0C;
constexpr uint8_t KEY_J = 0x0D;
constexpr uint8_t KEY_K = 0x0E;
constexpr uint8_t KEY_L = 0x0F;
constexpr uint8_t KEY_M = 0x10;
constexpr uint8_t KEY_N = 0x11;
constexpr uint8_t KEY_O = 0x12;
constexpr uint8_t KEY_P = 0x13;
constexpr uint8_t KEY_Q = 0x14;
constexpr uint8_t KEY_R = 0x15;
constexpr uint8_t KEY_S = 0x16;
constexpr uint8_t KEY_T = 0x17;
constexpr uint8_t KEY_U = 0x18;
constexpr uint8_t KEY_V = 0x19;
constexpr uint8_t KEY_W = 0x1A;
constexpr uint8_t KEY_X = 0x1B;
constexpr uint8_t KEY_Y = 0x1C;
constexpr uint8_t KEY_Z = 0x1D;
constexpr uint8_t KEY_1 = 0x1E;
constexpr uint8_t KEY_2 = 0x1F;
constexpr uint8_t KEY_3 = 0x20;
constexpr uint8_t KEY_4 = 0x21;
constexpr uint8_t KEY_5 = 0x22;
constexpr uint8_t KEY_6 = 0x23;
constexpr uint8_t KEY_7 = 0x24;
constexpr uint8_t KEY_8 = 0x25;
constexpr uint8_t KEY_9 = 0x26;
constexpr uint8_t KEY_0 = 0x27;
constexpr uint8_t KEY_ENTER = 0x28;
constexpr uint8_t KEY_BACKSPACE = 0x2A;
constexpr uint8_t KEY_SPACE = 0x2C;
constexpr uint8_t KEY_MINUS = 0x2D;        // - and _
constexpr uint8_t KEY_EQUAL = 0x2E;        // = and +
constexpr uint8_t KEY_LEFTBRACE = 0x2F;    // [ and {
constexpr uint8_t KEY_RIGHTBRACE = 0x30;   // ] and }
constexpr uint8_t KEY_BACKSLASH = 0x31;    // \ and |
constexpr uint8_t KEY_SEMICOLON = 0x33;    // ; and :
constexpr uint8_t KEY_APOSTROPHE = 0x34;   // ' and "
constexpr uint8_t KEY_GRAVE = 0x35;        // ` and ~
constexpr uint8_t KEY_COMMA = 0x36;        // , and <
constexpr uint8_t KEY_DOT = 0x37;          // . and >
constexpr uint8_t KEY_SLASH = 0x38;        // / and ?


static const std::map<uint16_t, KeyInfo> us_layout_mapping = {
    // LVGL Keyboard Layout - mapped from lv_keyboard.c source code
    // See: lvgl/src/extra/widgets/keyboard/lv_keyboard.c
    //
    // TEXT_LOWER mode (default_kb_map_lc):
    // Row 1: "1#", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", BACKSPACE
    // Row 2: "ABC", "a", "s", "d", "f", "g", "h", "j", "k", "l", ENTER
    // Row 3: "_", "-", "z", "x", "c", "v", "b", "n", "m", ".", ",", ":"
    // Row 4: KEYBOARD, LEFT, " ", RIGHT, OK

    // Row 1: q w e r t y u i o p (btn_id 1-10)
    {1, {KEY_Q, 0}},
    {2, {KEY_W, 0}},
    {3, {KEY_E, 0}},
    {4, {KEY_R, 0}},
    {5, {KEY_T, 0}},
    {6, {KEY_Y, 0}},
    {7, {KEY_U, 0}},
    {8, {KEY_I, 0}},
    {9, {KEY_O, 0}},
    {10, {KEY_P, 0}},

    // Row 2: a s d f g h j k l (btn_id 13-21)
    {13, {KEY_A, 0}},
    {14, {KEY_S, 0}},
    {15, {KEY_D, 0}},
    {16, {KEY_F, 0}},
    {17, {KEY_G, 0}},
    {18, {KEY_H, 0}},
    {19, {KEY_J, 0}},
    {20, {KEY_K, 0}},
    {21, {KEY_L, 0}},

    // Row 3: _ - z x c v b n m . , : (btn_id 23-34)
    {23, {KEY_MINUS, KEY_MOD_LSHIFT}},  // _
    {24, {KEY_MINUS, 0}},                // -
    {25, {KEY_Z, 0}},
    {26, {KEY_X, 0}},
    {27, {KEY_C, 0}},
    {28, {KEY_V, 0}},
    {29, {KEY_B, 0}},
    {30, {KEY_N, 0}},
    {31, {KEY_M, 0}},
    {32, {KEY_DOT, 0}},                  // .
    {33, {KEY_COMMA, 0}},                // ,
    {34, {KEY_SEMICOLON, KEY_MOD_LSHIFT}}, // : (shift + ;)

    // Row 4: Space (btn_id 37)
    {37, {KEY_SPACE, 0}},

    // When keyboard mode is SPECIAL (after pressing "1#"), the button IDs are reassigned:
    // SPECIAL mode (default_kb_map_spec):
    // Row 1: "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", BACKSPACE (btn_id 0-10)
    // Row 2: "abc", "+", "&", "/", "*", "=", "%", "!", "?", "#", "<", ">" (btn_id 12-23)
    // Row 3: "\", "@", "$", "(", ")", "{", "}", "[", "]", ";", "\"", "'" (btn_id 24-35)
    // Row 4: KEYBOARD, LEFT, " ", RIGHT, OK (btn_id 36-40)

    // NOTE: btn_id 0-10 are overloaded between modes!
    // In SPECIAL mode: 0="1", 1="2", etc.
    // In TEXT mode: 0="1#", 1="q", etc.
    // We'll handle this by checking the keyboard mode in the event handler
};

static const KeyboardLayout USKeyboardLayout(us_layout_mapping);
