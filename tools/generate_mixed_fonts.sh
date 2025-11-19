#!/bin/bash

# Script to generate LVGL fonts with Montserrat (text) + NotoSansSymbols2 (emoji)
# This creates fonts that support both regular text AND emoji properly

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MONTSERRAT_FONT="$SCRIPT_DIR/fonts/Montserrat-Regular.ttf"
EMOJI_FONT="$SCRIPT_DIR/fonts/NotoSansSymbols2-Regular.ttf"
OUTPUT_DIR="$SCRIPT_DIR/../lib/fonts"
LV_FONT_CONV="${HOME}/.npm-global/bin/lv_font_conv"

# ASCII + Latin-1 Supplement for text (from Montserrat)
TEXT_RANGES="0x20-0x7E,0xA0-0xFF"

# Emoji and symbols only (from NotoSansSymbols2)
# Excludes ASCII to avoid conflicts with Montserrat
EMOJI_RANGES="0x23F0-0x23F3,0x2600-0x2604,0x2614-0x2615,0x261D,0x2665-0x2666,0x26A0-0x26A1,0x26AA-0x26AB,0x26C4-0x26C5,0x2753-0x2755,0x2757,0x2764,0x27A1,0x2B05-0x2B07,0x2B1B-0x2B1C,0x2B50,0x2B55,0x1F300-0x1F321,0x1F324-0x1F393,0x1F3E0,0x1F400-0x1F4FD,0x1F4FF-0x1F53D,0x1F550-0x1F567,0x1F573-0x1F57A,0x1F587,0x1F58A-0x1F58D,0x1F5A4-0x1F5A5,0x1F5A8,0x1F5B1-0x1F5B2,0x1F5BC-0x1F5C4,0x1F5D1-0x1F5D3,0x1F5E1,0x1F5E3,0x1F5E8,0x1F5FA-0x1F64F,0x1F680-0x1F6C5,0x1F6CB-0x1F6D2,0x1F6F3-0x1F6FC,0x1F7E0-0x1F7EB"

# Check if fonts exist
if [ ! -f "$MONTSERRAT_FONT" ]; then
    echo "Error: Montserrat font not found at $MONTSERRAT_FONT"
    echo "Download from: https://fonts.google.com/specimen/Montserrat"
    exit 1
fi

if [ ! -f "$EMOJI_FONT" ]; then
    echo "Error: Emoji font not found at $EMOJI_FONT"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "=============================================="
echo "LVGL Mixed Font Generation Script"
echo "=============================================="
echo "Text font (ASCII): $MONTSERRAT_FONT"
echo "Emoji font: $EMOJI_FONT"
echo "Output directory: $OUTPUT_DIR"
echo "=============================================="
echo ""

# Function to generate a mixed font
generate_mixed_font() {
    local size=$1
    local name="montserrat_emoji_${size}"
    local output_file="$OUTPUT_DIR/${name}.c"

    echo "Generating ${name}.c (${size}px) - Montserrat + Emoji..."

    "$LV_FONT_CONV" \
        --font "$MONTSERRAT_FONT" \
        --size "$size" \
        --format lvgl \
        --bpp 4 \
        --no-compress \
        --range "$TEXT_RANGES" \
        --font "$EMOJI_FONT" \
        --size "$size" \
        --range "$EMOJI_RANGES" \
        --output "$output_file" \
        --force-fast-kern-format

    if [ $? -eq 0 ]; then
        echo "✓ ${name}.c generated successfully"
        ls -lh "$output_file"
    else
        echo "✗ Failed to generate ${name}.c"
        return 1
    fi
    echo ""
}

# Generate fonts
echo "Starting font generation..."
echo ""

generate_mixed_font 14
generate_mixed_font 20
generate_mixed_font 24

echo "=============================================="
echo "Font generation completed!"
echo "=============================================="
echo ""
echo "Generated files in $OUTPUT_DIR:"
ls -lh "$OUTPUT_DIR"/montserrat_emoji_*.c 2>/dev/null || echo "No files generated"
echo ""
echo "To use these fonts in your LVGL code:"
echo "1. Include the header: LV_FONT_DECLARE(montserrat_emoji_14);"
echo "2. Set font: lv_obj_set_style_text_font(obj, &montserrat_emoji_14, 0);"
echo ""
echo "These fonts support:"
echo "  - ASCII text (A-Z, a-z, 0-9, symbols) from Montserrat"
echo "  - Emoji (🏠⚡📊💾🔧 etc.) from NotoSansSymbols2"
echo ""
