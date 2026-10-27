# Bramley

An Apple-inspired OS for the TrimUI Brick Hammer and Brick, forked from [NextUI](https://github.com/LoveRetro/NextUI). It has an Apple TV style home screen, a music player, and Settings redrawn in the style of iPad Settings. It is free and not for sale.

Bramley was called Brick Hammer Launcher until October 2026. **[Download the latest release](https://github.com/howardhwt/bramley/releases)**. Version 0.1.1, released under the old name, is a pre-release tested on one Brick Hammer with NextUI 6.15.0. Bramley 0.2 is in progress.

| Home | Game library |
|---|---|
| ![Home screen with the Top Shelf and Continue Playing](/docs/screenshots/home.png) | ![Game library grid for one console](/docs/screenshots/library.png) |
| **Now Playing** | **Quick menu** |
| ![Now Playing over a blurred background](/docs/screenshots/nowplaying.png) | ![Quick menu with Wi-Fi and Bluetooth tiles](/docs/screenshots/quickmenu.png) |

The screenshots come from the desktop build with demo content. The games and albums in them are invented.

## What it does

- **Home screen.** A large Top Shelf shows the focused item: its art, its console, and buttons to play or resume a save. Below it are Continue Playing and Consoles rows, and Apps as a grid. The focused item lifts with a spring and a sweep of light, and the wallpaper shifts slightly as you move.
- **Game library.** Each console opens a grid of box art. Games without art get a title card. L1 and R1 jump A to Z.
- **Quick menu.** MENU opens a panel over a blurred, frozen copy of the screen, with Wi-Fi, Bluetooth, Settings, Sleep, Restart and Shut Down. Wi-Fi and Bluetooth switch in the background and show their status live.
- **Music.** A Music app plays albums and audiobooks, with a bookmark and speed for each book. SELECT opens Now Playing from anywhere. Starting a game pauses the music, and SELECT resumes it afterwards. Music keeps playing when you stop touching the device.
- **Lock screen.** While music plays, a short press of power turns the screen off and the music keeps going. Power again shows the lock screen, and power once more unlocks. Holding L3 and R3 for a second, or X in Now Playing, locks too. Holding power still turns the device off, and the music remembers where it was.
- **Collections.** NextUI's collections appear as their own row, in your own order.
- **Low battery.** A warning at 5 percent, and a clean power-off at 2 percent instead of the device dying.
- **Chinese, Japanese and Korean names** are drawn with Source Han Sans from the device's own firmware.
- **Settings.** The sections sit in a sidebar with iOS-style icons, and the focused section's settings show live beside it, as on an iPad. On and off settings are switches that A flips, and levels such as volume are sliders.
- **Wallpapers.** Eight wallpapers made for Bramley come with it.
- **Faster switching.** Moving between the home screen, games and Settings no longer waits on a slow screen clear.
- **Safety net.** If the launcher fails to start twice in a row, stock NextUI opens instead, so a bad build never locks you out.

## Install

1. NextUI must already be installed on the card.
2. Extract the release zip to the root of the SD card. This creates `Tools/tg5040/Bramley.pak`.
3. On the device, open Tools, then Bramley. The new home screen appears.

Bramley replaces NextUI's launcher, Settings and emulator, and keeps the stock ones beside them. To remove it, open the Bramley Pak again from Apps; stock NextUI comes back, and your music bookmarks are kept. After a NextUI update the stock programs may return; open the Pak again to reinstall. Installing over Brick Hammer Launcher updates it in place, and the old `Brick Hammer Launcher.pak` folder can then be deleted.

## Buttons

| Where | Button | Action |
|---|---|---|
| Everywhere | D-pad | Move |
| | A | Open, or play |
| | B | Back |
| | MENU | Quick menu |
| | SELECT | Now Playing |
| Continue Playing | X | Resume the last manual save |
| Game library | L1, R1 | Previous or next letter |
| Music | L1, R1 | Switch between Albums and Audiobooks |
| Music playing | Power (short press) | Lock the screen |
| | L3 + R3 (hold 1 s) | Lock or unlock the screen |
| Now Playing | A | Play or pause |
| | L1, R1 | Previous or next track, or chapter in a book |
| | Left, Right | Back or forward 10 seconds |
| | Y | Play mode, or speed for a book |
| | X | Lock the screen |

## Your card

| What | Where |
|---|---|
| Box art | `Roms/<Console>/.media/<game name>.png`, the same as stock NextUI |
| Console icons | `.media/consoles/<TAG>.png`, where TAG is the short name in brackets, for example `GBA` for `Game Boy Advance (GBA)` |
| Music | `Music/<Artist>/<Album>/` or `Music/<Album>/` |
| Audiobooks | `Audiobooks/<Author>/<Book>/` or `Audiobooks/<Book>/` |
| Album covers | `cover.jpg` or `folder.jpg` in the album folder, else the picture inside the first track |
| Collections | `Collections/<name>.txt`, one card path per line such as `/Roms/Game Boy Advance (GBA)/Game.gba`, the same as stock NextUI. They appear as their own row |
| Wallpaper | `bg.png` at the card root, the same as stock NextUI |
| Fonts | `Fonts/` at the card root (see below) |

Bramley never moves or renames your files on the card. Its own saved state lives in `.userdata/shared/.bramley/`.

### Fonts

Inter is included. To use a font you own, put its files in `Fonts/` at the card root, with Regular, SemiBold (or Medium) and Bold in the file names. Free fonts close to Apple's system font include Inter Display, Manrope and Public Sans. Characters the font lacks, such as Chinese, fall back to Source Han Sans from the firmware.

## Known limits

- There are no Bluetooth headset buttons and no online album art yet.
- FN1 and FN2 shortcuts run when the button is released, since L3 and R3 together lock the screen.
- While the screen is locked, NextUI's brightness shortcut can still turn the backlight on.
- In Settings, the on-screen keyboard and the colour picker still use NextUI's stock look.
- Save-state screenshots appear only for manual saves; NextUI does not take one for its automatic save.
- NextUI's simple mode is not supported yet.

## Building

The launcher lives in [`workspace/all/launcher`](/workspace/all/launcher), and the Settings drawing in [`workspace/all/settings/bramley_settings.cpp`](/workspace/all/settings/bramley_settings.cpp). The rest of the repository is NextUI, forked at upstream commit `5291344b`. Edits to NextUI's own files are marked with a `Bramley:` comment.

- **Release zip:** `sh workspace/all/launcher/package.sh`, which needs Docker or OrbStack for NextUI's tg5040 toolchain. It writes `releases/Bramley-<version>.zip`.
- **Desktop build:** the launcher builds with NextUI's `desktop` target for quick testing on a Mac. See `CLAUDE.md` for the steps, which avoid NextUI's macOS script that empties `/usr/local/bin`. `tools/fake-muse.py` stands in for the music daemon, and `tools/demo-sd.sh` fills the desktop's fake SD card with demo content.

## Credits and licenses

- **[NextUI](https://github.com/LoveRetro/NextUI)** by the LoveRetro team, under PolyForm Noncommercial 1.0.0. Bramley is therefore free only, and nothing in it may be sold.
- **Muse**, the music daemon, and its library and play-order code come from **[TortOS](https://github.com/ericreinsmidt/TortOS)** by Eric Reinsmidt, under the MIT license.
- **[Inter](https://github.com/rsms/inter)** by the Inter Project Authors, under the SIL Open Font License 1.1.
- Three Settings symbols come from **[Phosphor Icons](https://github.com/phosphor-icons/web)**, under the MIT license. The other symbols and the wallpapers were made for Bramley.

Full notices are in [`THIRD-PARTY-LICENSES.md`](/THIRD-PARTY-LICENSES.md). This project is not affiliated with Apple, TrimUI, or the NextUI team.
