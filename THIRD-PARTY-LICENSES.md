# Third-party licenses

This fork of NextUI is licensed under PolyForm Noncommercial 1.0.0 (see `LICENSE`). The components below keep their own licenses.

## TortOS Muse

- **Source:** https://github.com/ericreinsmidt/TortOS, tag v1.6.0, commit 48aaa4c753fa3683229fb203ce04d6ba4efc80de.
- **License:** MIT.
- **Copyright:** Copyright (c) 2026 Eric Reinsmidt.
- **Used in:**
  - `spikes/muse/src/musectl.c` is vendored unchanged. The `muse` binary is downloaded from the TortOS release at build time and is not committed.
  - `workspace/all/launcher/tortos/muselib.c`, `muselib.h`, `musequeue.c` and `musequeue.h` are vendored unchanged as the launcher's music library scan and play order. `tortos/library.h` is our own shim that supplies the one define muselib.h needs.
  - The launcher talks to the Muse daemon over its socket protocol (`workspace/all/launcher/lmuse.c`, our own code).
  - The lock screen's chord handling in `workspace/all/launcher/launcher.c` (`lock_chord`: lock once per hold, and only after both keys were released) is adapted from TortOS v1.6.0 `src/main.c`.
  - TortOS v2.0.0 and later are GPL-3.0-or-later and are not used: GPL code cannot be combined with NextUI's PolyForm Noncommercial license.

```
MIT License

Copyright (c) 2026 Eric Reinsmidt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Phosphor Icons

- **Source:** https://github.com/phosphor-icons/web, npm package `@phosphor-icons/web` 2.1.2 (SHA-256 `40e3096099ca818c047979cece6a3764944d5200c67b64092f7bcd8bcd1d2b08`).
- **License:** MIT.
- **Copyright:** Copyright (c) 2020-2021 Phosphor Icons.
- **Used in:** the palette, sun and info symbols on the Settings icons in `workspace/all/launcher/res/settings/`, rendered by `workspace/all/launcher/tools/settings-icons.py`. The font is downloaded at render time and is not committed. The other symbols and all the wallpapers in `res/wallpapers/` are our own work.

```
MIT License

Copyright (c) 2020-2021 Phosphor Icons

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Inter

- **Source:** https://github.com/rsms/inter, release v4.1.
- **License:** SIL Open Font License 1.1. The full text is in `workspace/all/launcher/res/Inter-LICENSE.txt`.
- **Copyright:** Copyright (c) 2016 The Inter Project Authors.
- **Used in:** `workspace/all/launcher/res/Inter-SemiBold.ttf` and `Inter-Bold.ttf`, unchanged, as the launcher's UI font.

## Emulator cores

- **Source:** the upstream repos and exact commits are listed in `CORES-SOURCE.md` on each Bramley card, made by `workspace/all/launcher/cores-source.sh` from the pins in `workspace/tg5040/cores/makefile`. Bramley's patches are in `workspace/tg5040/cores/patches/` and `workspace/all/cores/patches/`.
- **Licenses:** mostly GPL-2.0, with GPL-3.0 (PokeMini, Gearcoleco), MPL-2.0 (mGBA), zlib (Handy), MIT (fake-08), and noncommercial licenses (Snes9x, PicoDrive, FBNeo). Each core's own license files are copied to `Licenses/<core>/` on the card. The survey is in `docs/plans/cores-licenses.md`.
- **Conditions:** Bramley stays free, with no sale, ads or donation requests (FBNeo forbids asking for donations), and each release publishes the source of the cores it ships.

## Material Symbols

- **Source:** https://github.com/google/material-design-icons, commit 10ecc14790612ca8fd164e3f4ffe9c16fc50d5b7, the variable font `MaterialSymbolsRounded`.
- **License:** Apache License 2.0 (https://www.apache.org/licenses/LICENSE-2.0).
- **Copyright:** Google LLC.
- **Used in:** ten symbols (stadia_controller, album, joystick, keyboard, star, format_list_bulleted, music_note, bar_chart, folder, apps), rendered by `workspace/all/launcher/tools/console-glyphs.py` into the console and app icons in `workspace/all/launcher/res/brand/consoles/`. The symbols are drawn white on Bramley's coloured tiles; no other change is made to them.
