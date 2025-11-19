#!/bin/bash

# Font Download Script for ESP32-S3 LVGL Project
# Downloads recommended TTF fonts from Google Fonts and GitHub

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
FONTS_DIR="$PROJECT_DIR/data/fonts"

echo "=== Font Download Script ==="
echo "Fonts will be downloaded to: $FONTS_DIR"
echo ""

# Create fonts directory
mkdir -p "$FONTS_DIR"
cd "$FONTS_DIR"

# Download Inter (modern, legible sans-serif)
echo "[1/6] Downloading Inter..."
if [ ! -f "Inter-Regular.ttf" ]; then
    curl -L -o Inter.zip "https://github.com/rsms/inter/releases/download/v4.0/Inter-4.0.zip"
    unzip -q Inter.zip "Inter Desktop/*.ttf" -d inter_tmp
    mv inter_tmp/Inter\ Desktop/Inter-Regular.ttf .
    mv inter_tmp/Inter\ Desktop/Inter-Bold.ttf .
    mv inter_tmp/Inter\ Desktop/Inter-Light.ttf .
    rm -rf inter_tmp Inter.zip
    echo "✓ Inter fonts downloaded"
else
    echo "⊙ Inter fonts already exist"
fi

# Download Montserrat (clean sans-serif)
echo "[2/6] Downloading Montserrat..."
if [ ! -f "Montserrat-Regular.ttf" ]; then
    curl -L -o Montserrat-Regular.ttf "https://github.com/JulietaUla/Montserrat/raw/master/fonts/ttf/Montserrat-Regular.ttf"
    curl -L -o Montserrat-Bold.ttf "https://github.com/JulietaUla/Montserrat/raw/master/fonts/ttf/Montserrat-Bold.ttf"
    curl -L -o Montserrat-Light.ttf "https://github.com/JulietaUla/Montserrat/raw/master/fonts/ttf/Montserrat-Light.ttf"
    echo "✓ Montserrat fonts downloaded"
else
    echo "⊙ Montserrat fonts already exist"
fi

# Download Source Sans 3
echo "[3/6] Downloading Source Sans 3..."
if [ ! -f "SourceSans3-Regular.ttf" ]; then
    curl -L -o SourceSans3.zip "https://github.com/adobe-fonts/source-sans/releases/download/3.052R/TTF-source-sans-3.052R.zip"
    unzip -q SourceSans3.zip "TTF/*.ttf" -d sourcesans_tmp
    mv sourcesans_tmp/TTF/SourceSans3-Regular.ttf .
    mv sourcesans_tmp/TTF/SourceSans3-Bold.ttf .
    mv sourcesans_tmp/TTF/SourceSans3-Light.ttf .
    rm -rf sourcesans_tmp SourceSans3.zip
    echo "✓ Source Sans 3 fonts downloaded"
else
    echo "⊙ Source Sans 3 fonts already exist"
fi

# Download Roboto Slab (serif for headings)
echo "[4/6] Downloading Roboto Slab..."
if [ ! -f "RobotoSlab-Regular.ttf" ]; then
    curl -L -o RobotoSlab.zip "https://github.com/googlefonts/RobotoSerif/releases/download/v1.000854/RobotoSlab-fonts.zip"
    unzip -q RobotoSlab.zip "fonts/ttf/RobotoSlab-Regular.ttf" "fonts/ttf/RobotoSlab-Bold.ttf" -d robotoslab_tmp
    mv robotoslab_tmp/fonts/ttf/RobotoSlab-Regular.ttf .
    mv robotoslab_tmp/fonts/ttf/RobotoSlab-Bold.ttf .
    rm -rf robotoslab_tmp RobotoSlab.zip
    echo "✓ Roboto Slab fonts downloaded"
else
    echo "⊙ Roboto Slab fonts already exist"
fi

# Download Playfair Display (elegant serif)
echo "[5/6] Downloading Playfair Display..."
if [ ! -f "PlayfairDisplay-Regular.ttf" ]; then
    curl -L -o PlayfairDisplay.zip "https://github.com/clauseggers/Playfair-Display/archive/refs/heads/master.zip"
    unzip -q PlayfairDisplay.zip "*/fonts/ttf/PlayfairDisplay-Regular.ttf" "*/fonts/ttf/PlayfairDisplay-Bold.ttf" -d playfair_tmp
    find playfair_tmp -name "PlayfairDisplay-Regular.ttf" -exec mv {} . \;
    find playfair_tmp -name "PlayfairDisplay-Bold.ttf" -exec mv {} . \;
    rm -rf playfair_tmp PlayfairDisplay.zip
    echo "✓ Playfair Display fonts downloaded"
else
    echo "⊙ Playfair Display fonts already exist"
fi

# Download Noto Color Emoji (color emoji support)
echo "[6/6] Downloading Noto Color Emoji..."
echo "⚠ WARNING: NotoColorEmoji.ttf is ~10MB - too large for ESP32-S3 flash!"
echo "   Skipping download. Use PNG emoji with imgfont instead."
# Uncomment if you want to test locally:
# if [ ! -f "NotoColorEmoji.ttf" ]; then
#     curl -L -o NotoColorEmoji.ttf "https://github.com/googlefonts/noto-emoji/raw/main/fonts/NotoColorEmoji.ttf"
#     echo "✓ Noto Color Emoji downloaded"
# else
#     echo "⊙ Noto Color Emoji already exists"
# fi

echo ""
echo "=== Download Complete ==="
echo "Fonts available:"
ls -lh "$FONTS_DIR" | grep -E "\.ttf$" || echo "No TTF files found"
echo ""
echo "Next steps:"
echo "1. Verify fonts are in data/fonts/"
echo "2. Upload to ESP32-S3: pio run --target uploadfs"
echo "3. Use FontManager class to load fonts dynamically"
echo ""
echo "Total size:"
du -sh "$FONTS_DIR"
