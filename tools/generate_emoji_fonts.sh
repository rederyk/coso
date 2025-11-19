#!/bin/bash

# Script to generate LVGL emoji fonts with Noto Emoji
# Generated for ESP32-S3 project with LVGL 8.4.0

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FONT_FILE="$SCRIPT_DIR/fonts/NotoSansSymbols2-Regular.ttf"
OUTPUT_DIR="$SCRIPT_DIR/../lib/fonts"
LV_FONT_CONV="${HOME}/.npm-global/bin/lv_font_conv"

# Unicode ranges verified as available in NotoSansSymbols2
# Includes ASCII, symbols, and a comprehensive set of emoji
# This covers most of the requested symbols: 🏠⚙️🛰️ℹ️❌⚠️📶💡🖥️⚡📊💾🔧🔄👆🖌️ and more
RANGES="0x20-0x7E,0x23F0-0x23F3,0x2600-0x2604,0x2614-0x2615,0x261D,0x2665-0x2666,0x26A0-0x26A1,0x26AA-0x26AB,0x26C4-0x26C5,0x2753-0x2755,0x2757,0x2764,0x27A1,0x2B05-0x2B07,0x2B1B-0x2B1C,0x2B50,0x2B55,0x1F300-0x1F321,0x1F324-0x1F393,0x1F3E0,0x1F400-0x1F4FD,0x1F4FF-0x1F53D,0x1F550-0x1F567,0x1F573-0x1F57A,0x1F587,0x1F58A-0x1F58D,0x1F5A4-0x1F5A5,0x1F5A8,0x1F5B1-0x1F5B2,0x1F5BC-0x1F5C4,0x1F5D1-0x1F5D3,0x1F5E1,0x1F5E3,0x1F5E8,0x1F5FA-0x1F64F,0x1F680-0x1F6C5,0x1F6CB-0x1F6D2,0x1F6F3-0x1F6FC,0x1F7E0-0x1F7EB"

# Check if font file exists
if [ ! -f "$FONT_FILE" ]; then
    echo "Error: Font file not found at $FONT_FILE"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "=============================================="
echo "LVGL Emoji Font Generation Script"
echo "=============================================="
echo "Font source: $FONT_FILE"
echo "Output directory: $OUTPUT_DIR"
echo "=============================================="
echo ""

# Function to generate a font
generate_font() {
    local size=$1
    local name="emoji_${size}"
    local output_file="$OUTPUT_DIR/${name}.c"

    echo "Generating ${name}.c (${size}px)..."

    "$LV_FONT_CONV" \
        --font "$FONT_FILE" \
        --size "$size" \
        --format lvgl \
        --bpp 4 \
        --no-compress \
        --range "$RANGES" \
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

# Generate all 5 fonts
echo "Starting font generation..."
echo ""

generate_font 14
generate_font 20
generate_font 24
generate_font 48
generate_font 96

echo "=============================================="
echo "Font generation completed!"
echo "=============================================="
echo ""
echo "Generated files in $OUTPUT_DIR:"
ls -lh "$OUTPUT_DIR"/*.c
echo ""
echo "To use these fonts in your LVGL code:"
echo "1. Include the header: LV_FONT_DECLARE(emoji_14);"
echo "2. Set font: lv_obj_set_style_text_font(obj, &emoji_14, 0);"
echo ""
