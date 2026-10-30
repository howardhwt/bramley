# Building Bramley

Bramley builds like NextUI: the device programs and the emulator cores compile inside NextUI's tg5040 toolchain container, and a desktop build runs the launcher and Settings in a window on macOS for work on the interface.

## What you need

- Docker or [OrbStack](https://orbstack.dev) (the toolchain image is `ghcr.io/loveretro/tg5040-toolchain:latest`).
- `git`, `curl`, `zip`, `unzip` and `shasum` (all standard on macOS).
- For the desktop build: Homebrew with `gcc make cmake sdl2_image sdl2_ttf libsamplerate libzip sqlite`.

## 1. The emulator cores

Every core is pinned to an exact upstream commit in `workspace/tg5040/cores/makefile`, and Bramley's patches are in `workspace/tg5040/cores/patches/` and `workspace/all/cores/patches/`. Build them once (it takes a while), and again only when a pin or patch changes:

```sh
docker run --rm -v "$PWD/workspace":/root/workspace ghcr.io/loveretro/tg5040-toolchain:latest \
  /bin/bash -c '. ~/.bashrc && export PLATFORM=tg5040 UNION_PLATFORM=tg5040 && cd /root/workspace/tg5040/cores && make'
```

The cores land in `workspace/tg5040/cores/output/`.

## 2. A whole card

```sh
sh workspace/all/launcher/card.sh
```

This builds Bramley's programs (the launcher with Settings inside it, the emulator, the settings library), takes NextUI 6.15.0's release as the base (downloaded once and checked against its SHA-256), puts Bramley's programs, cores and assets in place, and writes to `releases/`:

- `Bramley-<version>-card.zip`, the whole card.
- `Bramley-<version>-update.zip`, the system zip only (`MinUI.zip`).
- `bramley-update.txt`, the manifest Software Update reads.

The version comes from `workspace/all/launcher/VERSION`. `BRAMLEY_VERSION=0.9.9 sh workspace/all/launcher/card.sh` overrides it for a test build.

## 3. Trying a build on a device

Over USB with `adb`, copy only the files you changed, then restart the launcher without killing it:

```sh
adb push workspace/all/launcher/build/tg5040/launcher.elf /mnt/SDCARD/.system/tg5040/bin/launcher.elf
adb shell touch /tmp/launcher_reload
```

Never `kill` the launcher: the stop signal reaches NextUI's input code as a quit, which shuts the device down.

To test Software Update without publishing, serve the release files from your computer and put its address in `/mnt/SDCARD/.userdata/shared/.bramley/update/test-server.txt` on the card (for example `http://192.168.1.20:8000`).

## 4. The desktop build (macOS)

Create the fake SD card once:

```sh
bash workspace/desktop/prepare_fake_sd_root.sh   # writes /var/tmp/nextui/sdcard
```

Build with Homebrew's GCC (linked as `gcc` and `g++` in a folder of your choice), for example:

```sh
cd workspace/all/launcher
gmake PLATFORM=desktop CROSS_COMPILE=<folder-with-gcc>/ PREFIX=/opt/homebrew PREFIX_LOCAL=/var/tmp/nextui \
  UNION_PLATFORM=desktop UNAME_S=Darwin
DYLD_LIBRARY_PATH=/var/tmp/nextui/lib ./build/desktop/launcher.elf
```

Keys: the arrows are the D-pad, S is A, A is B, Return is START and Space is MENU.

Do not run `workspace/desktop/macos_create_gcc_symlinks.sh`: it removes everything in `/usr/local/bin`.

Development switches for the desktop build:

- `LAUNCHER_DEBUG_KEYS="R R D a S"` presses keys in order and `S` saves a screenshot (with `LAUNCHER_DEBUG_SHOT=<prefix>`). Settings run on its own uses `BRAMLEY_DEBUG_KEYS` and `BRAMLEY_DEBUG_SHOT`.
- `LAUNCHER_RECORD=out.mp4` records every frame into a video.
- `BRAMLEY_FAKE_UPDATE=available` (or `downloading`, `ready`, `current`, `checking`, `failed`) shows Software Update in that state.
- `spikes/demo-library/make.sh` fills the fake card with free homebrew games for screenshots.

## Licence

Bramley is under the PolyForm Noncommercial License 1.0.0. See the [README](../README.md#licences-and-credits).
