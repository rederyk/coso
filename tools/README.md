# Font Generation Tools

This directory contains scripts and fonts for generating custom LVGL emoji fonts.

## Quick Start

```bash
# Generate all 5 emoji fonts (14, 20, 24, 48, 96 px)
./tools/generate_emoji_fonts.sh

# Test which Unicode ranges are available
./tools/test_font_ranges.sh
./tools/test_more_ranges.sh
```

## Files

- **generate_emoji_fonts.sh** - Main script to generate all 5 font files
- **test_font_ranges.sh** - Tests basic Unicode range availability
- **test_more_ranges.sh** - Tests extended emoji ranges
- **fonts/** - Directory containing source font files
  - `NotoSansSymbols2-Regular.ttf` - Primary font (642KB)
  - Other fonts (for testing/backup)

## Generated Output

Fonts are generated to: `lib/fonts/`

- emoji_14.c (123KB)
- emoji_20.c (209KB)
- emoji_24.c (284KB)
- emoji_48.c (999KB)
- emoji_96.c (3.8MB)

## Customization

Edit `generate_emoji_fonts.sh` to customize:

### Change Font Sizes

```bash
# Around line 64-68
generate_font 14
generate_font 20
generate_font 24
generate_font 48
generate_font 96

# Add new size:
generate_font 32  # Add this line
```

### Change Unicode Ranges

```bash
# Around line 16
RANGES="0x20-0x7E,0x2600-0x2604,..."  # Edit this

# Example: Only ASCII + few emoji
RANGES="0x20-0x7E,0x1F3E0,0x26A0,0x1F4CA"
```

### Change Output Directory

```bash
# Line 10
OUTPUT_DIR="$SCRIPT_DIR/../lib/fonts"  # Change this path
```

### Change BPP (Bits Per Pixel)

```bash
# Line 45
--bpp 4 \  # Change to 1, 2, or 8

# BPP Guide:
# 1 = monochrome (smallest files)
# 2 = 4 gray levels
# 4 = 16 gray levels (current, good balance)
# 8 = 256 gray levels (best quality, largest files)
```

## Prerequisites

- Node.js (v25+)
- npm
- lv_font_conv (installed via: `npm install -g lv_font_conv`)

Check installation:
```bash
node -v
lv_font_conv --version  # Should be 1.5.3+
```

## Testing Individual Characters

To test if a specific emoji is available:

```bash
# Test single emoji
~/.npm-global/bin/lv_font_conv \
  --font tools/fonts/NotoSansSymbols2-Regular.ttf \
  --size 20 \
  --format lvgl \
  --bpp 4 \
  --range 0x1F3E0 \  # House emoji
  --output /tmp/test.c

# If successful, the emoji is available
# If error "doesn't have any characters", it's not in the font
```

## Finding Unicode Values

Use Python to get Unicode value of an emoji:

```python
# In Python shell or script:
emoji = "🏠"
print(f"U+{ord(emoji):04X}")  # Output: U+1F3E0
print(hex(ord(emoji)))          # Output: 0x1f3e0
```

Or use online tools:
- https://unicode-table.com
- https://emojipedia.org

## Troubleshooting

### "Font doesn't have any characters" error

The specified Unicode range doesn't exist in the font. Try:
1. Run `./test_font_ranges.sh` to see available ranges
2. Check Unicode value is correct
3. Try a different source font

### "command not found: lv_font_conv"

Install lv_font_conv:
```bash
npm install -g lv_font_conv
```

If permission error:
```bash
mkdir ~/.npm-global
npm config set prefix '~/.npm-global'
echo 'export PATH=~/.npm-global/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
npm install -g lv_font_conv
```

### Fonts too large

1. Reduce Unicode ranges (keep only what you need)
2. Lower BPP (use 2 instead of 4)
3. Don't generate emoji_96 (save 3.8MB)
4. Use compression: remove `--no-compress` flag

### "bad interpreter" error

Fix line endings:
```bash
sed -i 's/\r$//' tools/generate_emoji_fonts.sh
chmod +x tools/generate_emoji_fonts.sh
```

## Advanced Usage

### Generate Single Font

```bash
~/.npm-global/bin/lv_font_conv \
  --font tools/fonts/NotoSansSymbols2-Regular.ttf \
  --size 20 \
  --format lvgl \
  --bpp 4 \
  --no-compress \
  --range 0x20-0x7E,0x1F3E0,0x26A0 \
  --output lib/fonts/my_custom_font.c \
  --force-fast-kern-format
```

### Use Multiple Source Fonts

Combine different fonts for different character ranges:

```bash
~/.npm-global/bin/lv_font_conv \
  --font Montserrat-Regular.ttf \
  --range 0x20-0x7E \
  --font NotoSansSymbols2-Regular.ttf \
  --range 0x1F300-0x1F6FF \
  --size 20 \
  --format lvgl \
  --bpp 4 \
  --output lib/fonts/mixed_font.c
```

### Add Font to Fallback Chain

```bash
~/.npm-global/bin/lv_font_conv \
  --font tools/fonts/NotoSansSymbols2-Regular.ttf \
  --size 20 \
  --format lvgl \
  --bpp 4 \
  --range 0x20-0x7E,0x1F300-0x1F6FF \
  --output lib/fonts/emoji_20.c \
  --force-fast-kern-format \
  --fallback lv_font_montserrat_20  # Fallback to built-in font
```

## lv_font_conv Documentation

Full options:
```
--font <path>              TTF/OTF font file (can specify multiple)
--size <pixels>            Font size in pixels
--format <fmt>             Output format: lvgl, bin, dump
--bpp <n>                  Bits per pixel: 1, 2, 3, 4, 8
--range <ranges>           Unicode ranges (e.g. 0x20-0x7F,0x1F300-0x1F320)
--symbols <string>         Alternative to --range, specify exact characters
--output <path>            Output file path
--no-compress              Disable compression
--force-fast-kern-format   Use fast kerning format
--lcd                      LCD subpixel rendering (horizontal)
--lcd-v                    LCD subpixel rendering (vertical)
--use-color-info           Preserve color information
--autohint-off             Disable auto hinting
--autohint-strong          Strong auto hinting
```

## Resources

- [LVGL Font Converter](https://lvgl.io/tools/fontconverter) - Online tool
- [lv_font_conv GitHub](https://github.com/lvgl/lv_font_conv) - CLI tool
- [LVGL Fonts Documentation](https://docs.lvgl.io/latest/en/html/overview/font.html)
- [Google Fonts Noto](https://fonts.google.com/noto)

## License

- NotoSansSymbols2: SIL Open Font License 1.1
- Scripts: Same as project license
- lv_font_conv: MIT License
