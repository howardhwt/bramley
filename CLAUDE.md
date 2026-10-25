# Brick Hammer launcher fork

An Apple-inspired launcher for NextUI on the TrimUI Brick Hammer (platform `tg5040`, same hardware as the TrimUI Brick). Public and free. The full design lives in the project TDD, section 12 ("Custom Launcher Fork"). This file holds the rules that matter while coding.

## Hard rules

- **Change only the launcher.** Our code lives in `workspace/all/nextui/` (the launcher, replacing `nextui.c`) and in new files we add. From v0.2 we also touch the drawing layer of `workspace/all/settings/menu.cpp`. Do not modify `minarch`, the cores, `btmanager`, Wi-Fi or Bluetooth logic, or other platforms. If a change elsewhere seems necessary, stop and ask.
- **Track upstream.** `upstream` points at `LoveRetro/NextUI`. Rebase on each upstream release. Keep diffs outside our files as small as possible so rebases stay cheap.
- **License.** NextUI is PolyForm Noncommercial 1.0.0. The fork is free only. Never add anything that sells, gates, or monetizes it.
- **Third-party code.** Muse code from TortOS (github.com/ericreinsmidt/TortOS) is MIT. Keep its SPDX headers and copyright lines, and list it in a `THIRD-PARTY-LICENSES.md`.
- **No Apple assets in the repo.** No SF Pro, SF Symbols, or Apple icons. Inter is the bundled font. The launcher loads SF Pro only if the user places it in a fonts folder on their card.
- **Device writes.** Never write to the SD card or device without saying which files and paths first.

## Design decisions (from TDD section 12)

- tvOS-style focus: the focused item lifts and scales with spring motion and parallax. D-pad only, no touch.
- Home screen: rows of Continue Playing, Consoles, and Apps.
- Game library: a horizontal row of box art per console, a styled title card when art is missing, shoulder buttons jump A to Z.
- Blur: live blur behind panels only if it holds 60 fps at 1024x768 on the device. Otherwise use a wallpaper blurred once at load.
- Music: port the Muse playback daemon and library from TortOS, redraw its UI inside the launcher. SELECT opens Now Playing from anywhere in the launcher. Music pauses when a game launches and offers to resume on return. Bluetooth headset buttons are deferred.
- v0.1: home rows, game library, launching games and Paks (stock Settings), music player. v0.2: Settings drawing reskin and a shared UI kit.

## Build and test

- Toolchain: NextUI builds inside Docker (OrbStack works). `make PLATFORM=tg5040 shell` enters the toolchain container. Check the NextUI README for the current full build steps.
- Desktop: the `desktop` platform target builds a window on macOS. Use it to tune motion and blur before going to the device.
  - Never run `workspace/desktop/macos_create_gcc_symlinks.sh`. It runs `rm -f /usr/local/bin/*`, which deletes Docker, OrbStack and other tools on Howard's Mac.
  - Instead, Homebrew gcc 16 is linked without sudo in `~/.local/nextui-gcc/`. Build with `gmake PLATFORM=desktop build CROSS_COMPILE=$HOME/.local/nextui-gcc/`. Homebrew needs `gcc make cmake sdl2_image sdl2_ttf libsamplerate libzip sqlite`.
  - Create the fake SD card once with `bash workspace/desktop/prepare_fake_sd_root.sh`, which writes `/var/tmp/nextui/sdcard`.
  - Run with `DYLD_LIBRARY_PATH=/var/tmp/nextui/lib` set. Keys: arrows are the D-pad, S is A, A is B, Return is START, Space is MENU.
- Deploy: push to the device with `adb` over USB-C. Push only the files being tested, and say which files and paths before each push. SD card swaps are the fallback.
- Test on the Mac first (desktop build, or an arm64 Linux container for device binaries) before anything goes to the device.
- Measure GPU cost on the device with stress runs that repeat the work K times per frame. On the GE8300, swap interval 0 is ignored and glFinish waits for the display, so fps and glFinish timing alone cannot show headroom.
- Spikes and experiments live in `spikes/`, never in the launcher source.

## Writing

Docs, commit messages, and anything written for Howard: plain sentences, no em dashes, no sentence fragments.
