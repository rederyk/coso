# Emoji Integration Status (WIP)

Emoji font integration is **not** currently active in the firmware. The UI uses the LVGL built-in FontAwesome symbols exposed via `src/ui/ui_symbols.h`, so there is no extra font footprint or licensing burden at runtime.

## What Exists
- Scripts to generate monochrome emoji fonts from `NotoSansSymbols2-Regular.ttf` (SIL OFL 1.1): `tools/generate_emoji_fonts.sh` and `tools/generate_mixed_fonts.sh`.
- Allowed source fonts kept in `tools/fonts/`: `NotoSansSymbols2-Regular.ttf` (emoji/symbols) and `Montserrat-Regular.ttf` (text for mixed builds).

## Removed/Unsupported
- Non-compliant fonts (`Symbola`, `Unifont`, `TwemojiMozilla`, `fa-solid-900`, `DejaVuSans`) were deleted to avoid licensing or size issues.
- No compiled emoji C arrays are present in `src/` and no `UI_EMOJI_*` helpers exist in code.
- Color emoji (`NotoColorEmoji.ttf`) is not shipped due to size and licensing overhead.

## How to Add Emoji Safely (Future Work)
1) Generate fonts from allowed sources only:
```bash
./tools/generate_emoji_fonts.sh        # NotoSansSymbols2 ranges
# or mixed text+emoji:
./tools/generate_mixed_fonts.sh
```
2) Copy only the sizes you need into `src/` and declare them with `LV_FONT_DECLARE(...)`.
3) Use UTF-8 strings (e.g., `"🏠"`). Remember that some glyphs like `⚙️` or `❌` are missing in NotoSansSymbols2; prefer `🔧` or `✖️` as listed in `docs/FONT_GENERATION_SUMMARY.md`.

## Licensing Notes
- Keep attribution for NotoSansSymbols2 and Montserrat (both SIL OFL 1.1) in `NOTICE`.
- Do not reintroduce GPL or proprietary fonts—keep the fonts directory clean to avoid license contamination.
