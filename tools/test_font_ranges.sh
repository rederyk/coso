#!/bin/bash

# Test which Unicode ranges are available in NotoSansSymbols2
FONT="tools/fonts/NotoSansSymbols2-Regular.ttf"
LV_FONT_CONV="$HOME/.npm-global/bin/lv_font_conv"

echo "Testing Unicode ranges in NotoSansSymbols2..."
echo ""

# Test individual ranges from the original spec
test_range() {
    local range=$1
    local desc=$2

    if "$LV_FONT_CONV" --font "$FONT" --size 14 --format lvgl --bpp 4 --no-compress --range "$range" --output /tmp/test_range.c --force-fast-kern-format 2>/dev/null; then
        echo "✓ $range - $desc"
        return 0
    else
        echo "✗ $range - $desc"
        return 1
    fi
}

# Test ranges
test_range "0x20-0x7E" "ASCII"
test_range "0x2139" "Info symbol"
test_range "0x2197-0x2199" "Arrows NE,SE,SW"
test_range "0x23F0-0x23F3" "Clocks"
test_range "0x2600-0x2604" "Weather"
test_range "0x2614-0x2615" "Umbrella, Coffee"
test_range "0x261D" "Pointing finger"
test_range "0x2638-0x263A" "Wheel, Smiley"
test_range "0x2640,0x2642" "Gender symbols"
test_range "0x2665-0x2666" "Heart, Diamond"
test_range "0x2692-0x2697" "Tools"
test_range "0x2699" "Gear"
test_range "0x26A0-0x26A1" "Warning, Lightning"
test_range "0x26AA-0x26AB" "Circles"
test_range "0x26C4-0x26C5" "Snowman, Sun"
test_range "0x2702,0x2705,0x270C,0x2714,0x2716" "Scissors, Check, Victory, Checkmark, X"
test_range "0x2728,0x2744,0x274C" "Sparkles, Snowflake, X"
test_range "0x2753-0x2755,0x2757" "Question marks, Exclamation"
test_range "0x2764" "Heart"
test_range "0x2795-0x2797" "Plus, Minus, Divide"
test_range "0x27A1" "Right arrow"
test_range "0x2B05-0x2B07" "Arrows"
test_range "0x2B1B-0x2B1C" "Squares"
test_range "0x2B50,0x2B55" "Star, Circle"
test_range "0x1F300-0x1F321" "Emoji weather/nature"
test_range "0x1F680-0x1F6C5" "Emoji transport"
test_range "0x1F7E0-0x1F7EB" "Colored shapes"

echo ""
echo "Test complete. Check results above."
