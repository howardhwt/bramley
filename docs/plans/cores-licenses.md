# Emulator cores: licenses and what a release owes

Status: survey, 2026-10-29 (v1.0 plan, step 3). Licenses were read from each upstream repo, not guessed. Nothing is changed yet.

The 28 cores built for tg5040 are listed in `workspace/tg5040/cores/makefile:1-28`, with repos at lines 41-131. Only three are pinned to a commit: fbneo, picodrive and pokemini (and libretro-uae to the 2.6.1 branch). The rest build from whatever the default branch is that day. The v1.0 card today reuses the cores inside NextUI 6.15.0's release zip.

| Core | Paks | Upstream | License | What we owe |
|---|---|---|---|---|
| fceumm | FC, FDS | libretro/libretro-fceumm | GPL-2.0 | source and patches |
| gambatte | GB, GBC | libretro/gambatte-libretro | GPL-2.0 | source and patches |
| gpsp | GBA | libretro/gpsp | GPL-2.0 | source and patches |
| picodrive | MD, SMS, GG, 32X, SEGACD, SG1000 | irixxxx/picodrive @b0be121 | own license: no sale or commercial use; modified builds need full source | free only; publish source |
| snes9x | SFC | libretro/snes9x | Snes9x license: noncommercial, keep notice | free only; keep notice |
| pcsx_rearmed | PS | libretro/pcsx_rearmed | GPL-2.0 | source and patches |
| stella2014 | A2600 | libretro/stella2014-libretro | GPL-2.0 | source and patches |
| a5200 | A5200 | libretro/a5200 | GPL-2.0 | source and patches |
| prosystem | A7800 | libretro/prosystem-libretro | GPL-2.0 | source and patches |
| handy | LYNX | libretro/libretro-handy | zlib | keep notice; mark changes |
| vice (x64, x128, xvic, xpet, xplus4) | C64, C128, VIC, PET, PLUS4 | libretro/vice-libretro | GPL-2.0 | source and patches |
| fake-08 | P8 | jtothebell/fake-08 | MIT (with WTFPL, zlib, MIT parts) | keep notices |
| fbneo | FBN | libretro/FBNeo @6a5cc25 | FBNeo license, plus MAME license | free only, no donations, changes public, license text verbatim |
| cap32 | CPC | libretro/libretro-cap32 | GPL-2.0 | source and patches; contains Amstrad ROMs |
| puae | PUAE | libretro/libretro-uae, branch 2.6.1 | GPL-2.0 | source and patches |
| beetle pce_fast | PCE | libretro/beetle-pce-fast-libretro | GPL-2.0 | source and patches |
| supafaust | SUPA | libretro/supafaust | GPL-2.0 | source and patches |
| beetle vb | VB | libretro/beetle-vb-libretro | GPL-2.0 | source and patches |
| mgba | MGBA, SGB | libretro/mgba | MPL-2.0 | offer source of MPL files |
| pokemini | PKM | libretro/PokeMini @78656d4 | GPL-3.0+ | source and patches |
| race | NGP, NGPC | libretro/RACE | GPL-2.0 | source and patches |
| prboom | PRBOOM | libretro/libretro-prboom | GPL-2.0 | source and patches |
| bluemsx | MSX | libretro/blueMSX-libretro | zlib-style with GPL parts | treat as GPL |
| gearcoleco | COLECO | drhelius/Gearcoleco | GPL-3.0 | source and patches |

No BIOS files ship. `Bios/PUAE/readme.txt` lists checksums only.

## Problems

1. **Exact source unknown.** 24 cores were built from an unrecorded commit, so we cannot publish the exact GPL source for the binaries in NextUI's zip. Fix: pin every core to a commit and build them ourselves.
2. **FBNeo forbids asking for donations** in any project using its code. Bramley must never add a donate or sponsor link while it ships FBNeo. Its MAME license part was not read (unverified).
3. **cap32 contains Amstrad's firmware** as headers. Amstrad's redistribution permission is widely cited but unverified here.
4. **minarch (PolyForm Noncommercial) loads GPL cores** into one process. MinUI and NextUI have always done this, but whether it makes a combined work under the GPL is unsettled. Needs a real opinion before a wide v1.0 release (unverified).
5. **GPLv3 cores** (PokeMini, Gearcoleco) may require installation information for a device image; users can already replace the core files on the card, which is likely enough (unverified).

## What each release needs

- The exact upstream commit of every core, our patches (`workspace/tg5040/cores/patches/`, `workspace/all/cores/patches/`) and the build scripts, published with the release (a tag in this repo plus source archives at the pinned commits).
- License texts on the card: GPL-2.0, GPL-3.0, MPL-2.0, FBNeo verbatim, Snes9x, PicoDrive, zlib notices, MIT notices, all listed in `THIRD-PARTY-LICENSES.md`.
- Free only: no sale, ads or donations.
