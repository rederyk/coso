#!/bin/bash
FONT="tools/fonts/NotoSansSymbols2-Regular.ttf"
LV_FONT_CONV="$HOME/.npm-global/bin/lv_font_conv"

test_range() {
    if "$LV_FONT_CONV" --font "$FONT" --size 14 --format lvgl --bpp 4 --no-compress --range "$1" --output /tmp/test_range.c --force-fast-kern-format 2>/dev/null; then
        echo "✓ $1 - $2"
    else
        echo "✗ $1 - $2"
    fi
}

echo "Testing additional emoji ranges..."
test_range "0x1F324-0x1F393" "More emoji (including 🏠)"
test_range "0x1F3A8" "Palette"
test_range "0x1F3E0" "House"
test_range "0x1F400-0x1F4FD" "Animals & Objects (including 📶💡🖥️⚡📊💾🔧)"
test_range "0x1F4FF-0x1F53D" "Objects"
test_range "0x1F550-0x1F567" "Clocks"
test_range "0x1F573-0x1F57A" "Misc"
test_range "0x1F587,0x1F58A-0x1F58D" "Pen, tools"
test_range "0x1F590,0x1F595-0x1F596" "Hand gestures"
test_range "0x1F5A4-0x1F5A5" "Devices"
test_range "0x1F5A8,0x1F5B1-0x1F5B2" "Printer, Mouse"
test_range "0x1F5BC-0x1F5C4" "Frame, Files"
test_range "0x1F5D1-0x1F5D3" "Trash, Files"
test_range "0x1F5E1,0x1F5E3,0x1F5E8" "Sword, Speaker, Balloon"
test_range "0x1F5FA-0x1F64F" "Map, Gestures (including 👆🖌️)"
test_range "0x1F6CB-0x1F6D2" "Couch, Shopping"
test_range "0x1F6F3-0x1F6FC" "Boat, etc"
test_range "0x1F90C-0x1F93A" "Gestures"
test_range "0x1F947-0x1F9FF" "More emoji"
