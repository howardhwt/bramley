# Bramley brand

Bramley is an Apple-inspired OS for the TrimUI Brick Hammer and Brick. This guide sets how it looks and sounds. The assets in this folder are made by `workspace/all/launcher/tools/brand.py`; change the script, not the files.

## The idea

Cut an apple across the middle and its seeds sit in a five-point star. The Bramley mark is those five seeds. The Bramley apple itself began as one pip planted in Southwell, England, in 1809, and that tree still stands.

The feel is premium and calm: black, glass and light, with one deep green that belongs to Bramley.

## Name

The name is **Bramley**. Use **Bramley OS** only where the kind of product needs saying, such as a store listing or the first line of a description. Never write BramleyOS, BRAMLEY in running text, or "the Bramley".

## Mark

The mark is five apple pips with their points meeting near the centre and a small gap between the points. It is never an apple, a leaf, a bite or anything else that could be taken for Apple's logo.

| File | Use |
|---|---|
| `bramley-mark.svg` | Ivory mark, for dark backgrounds |
| `bramley-mark-black.svg` | Black mark, for light backgrounds and print |
| `bramley-mark-white.svg` | White mark, on photos or on the orchard green |
| `bramley-icon-1024.png`, `-512`, `-150`, `-36` | The app icon: ivory mark on an orchard green glass tile |
| `bramley-lockup-stacked.png` | Icon over the wordmark: boot screen, splash, About |
| `bramley-lockup-horizontal.png` | Icon beside the wordmark: headers, the README |
| `bramley-bootlogo.bmp` | Boot logo, 1024x768, 24-bit, for the Brick's boot partition (`.png` copy for viewing): the bare mark in white, 150 px, centred 20 px above the middle, on black, as Apple starts its devices |
| `brand-sheet.png` | Everything on one page |

- Keep clear space around the icon of at least a quarter of its width.
- The icon is at least 20 px and the bare mark at least 16 px. Below that, use the wordmark alone.
- Do not rotate, stretch, outline, add effects to, or recolor the mark outside the colors below.
- Do not change the number of pips or their shape.

## Color

| Name | Hex | Use |
|---|---|---|
| Orchard | `#1E402C` | The icon's tile and brand surfaces |
| Orchard Night | `#08160F` | The tile's dark end |
| Ivory | `#F3EBD6` | The mark and the wordmark on dark |
| Accent | `#4AB278` | The interface: switches that are on, sliders, the selected item, progress, links |
| Alert | `#E5484D` | Warnings and errors only, such as low battery |
| Ink | `#0E0E12` | Backgrounds |

The interface has one accent, Accent green. Do not add Apple's blue or a second accent. Never use Accent for a warning, and never use Alert for anything else. Everyday interface text stays white on the dark glass; Ivory is for the brand.

## Type

Inter everywhere (SIL Open Font License, shipped in the Pak). No second typeface.

The wordmark is "Bramley" in title case: Inter Regular at boot-screen and lockup sizes, Inter SemiBold when it is small (below about 28 px), such as in About or a list. No spaced capitals, no all lower case.

## Voice

- Calm and plain. Say what happened and what to do next: "Game saved. Shutting down…"
- Specific: "Add shader files to the Shaders folder on the card, then open this menu again."
- Sentence case for labels, buttons and titles. No exclamation marks and no jokes.
- Name the thing, not the technology: "Set automatically", not "NTP".
- Short labels, full sentences in descriptions.
- American spelling: color, gray, center.

## Not ours

No Apple assets: no SF Pro, SF Symbols, Apple icons or Apple wallpapers. NextUI and other credits stay as set out in `THIRD-PARTY-LICENSES.md`. Bramley is free and is never sold.
