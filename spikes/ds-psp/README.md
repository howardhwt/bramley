# DS and PSP cores: build spike

This spike builds three libretro cores, and later the standalone DSperate emulator, for tg5040 so they can be measured on the Brick, as approved in `docs/plans/ds-psp.md` ("Answers (2026-11-05)"). Nothing here ships. The shipped cores makefile, `package.sh` and `card.sh` are untouched. Sources and binaries stay out of git through `.gitignore`; only this README, `build.sh`, `ice-test.sh`, `patches/` and `paks/` are meant to be tracked.

All three cores built on 2026-11-05.

| Core | Source | Commit | Stripped .so | Licence |
|---|---|---|---|---|
| melonDS | github.com/libretro/melonDS | `66b5d2634cd0a79030562811e6e05f5532f800ba` (2026-07-19) | `melonds_libretro.so`, 2,553,528 bytes (2.4 MB) | GPL-3.0 |
| DeSmuME 2015 | github.com/libretro/desmume2015 | `422b688009ccb9f2917c086a8fe79c8cecb030ae` (2026-08-23) | `desmume2015_libretro.so`, 2,375,384 bytes (2.3 MB) | GPL-2.0 (`desmume/COPYING`) |
| PPSSPP | github.com/hrydgard/ppsspp | `5b3ddcc0628585b7b091c9a9904dba58bb4118c8` (master, 2026-11-05) | `ppsspp_libretro.so`, 18,838,680 bytes (18 MB) | GPL-2.0 or later, plus bundled third-party code such as FFmpeg (LGPL) |

PPSSPP's submodules are pinned by that commit. The PPSSPP commit is a master commit, not a tagged release. A shipped build should move to the next tagged release and record it in `CORES-SOURCE.md`.

PPSSPP also needs its assets folder on the card. `out/PPSSPP/` holds a copy of `assets/` from the same commit (19 MB).

## How to build

From the repo root on the Mac, with OrbStack running:

```sh
git -C spikes/ds-psp/src clone https://github.com/libretro/melonDS melonds            # then check out the commit above
git -C spikes/ds-psp/src clone https://github.com/libretro/desmume2015 desmume2015    # then check out the commit above
git -C spikes/ds-psp/src clone https://github.com/hrydgard/ppsspp ppsspp             # then check out the commit and run
                                                                                      # git submodule update --init --recursive
docker run --rm -v "$PWD/workspace":/root/workspace -v "$PWD/spikes/ds-psp":/root/ds-psp \
  ghcr.io/loveretro/tg5040-toolchain:latest /bin/bash -c \
  '. ~/.bashrc && export PLATFORM=tg5040 UNION_PLATFORM=tg5040 && /root/ds-psp/build.sh melonds desmume2015 ppsspp'
```

The container runs natively as aarch64 under OrbStack, and its compiler is `aarch64-nextui-linux-gnu-gcc` 8.3.0 with a glibc 2.28 sysroot (the TrimUI SDK). melonDS and DeSmuME take about a minute each. PPSSPP takes several minutes.

## Flags

Every core uses `-mcpu=cortex-a53 -mtune=cortex-a53 -fomit-frame-pointer`, the CPU flags the shipped tg5040 core patches use. The output is stripped with `aarch64-nextui-linux-gnu-strip`. I left out `-ffast-math` and LTO, which some shipped cores use, because accuracy matters more than a few percent here and LTO on large C++ code is slow under GCC 8.

- melonDS: `make platform=unix ARCH=arm64`, with `CFLAGS` and `CXXFLAGS` passed through the environment so the Makefile appends to them. `ARCH=arm64` turns on the AArch64 JIT (`-DJIT_ENABLED`). `platform=unix` turns on threads (`-DHAVE_THREADS`). OpenGL stays off, so only the software renderer is built. The Makefile adds `-O3 -std=c++17`.
- DeSmuME 2015: `make -f Makefile.libretro platform=unix DESMUME_JIT=0 DESMUME_JIT_ARM=0`. DeSmuME 2015 has no AArch64 dynarec (its JITs are x86 and 32-bit ARM), so this build is the interpreter only. Expect it to be slow. The Makefile adds `-O2`.
- PPSSPP: CMake with `-DLIBRETRO=ON -DUSING_GLES2=ON -DUSING_EGL=OFF -DUSING_X11_VULKAN=OFF -DUSE_WAYLAND_WSI=OFF -DUSE_DISCORD=OFF -DUSE_MINIUPNPC=OFF -DUSE_FFMPEG=ON -DUSE_SYSTEM_FFMPEG=OFF`, at `-O3`, with the target `ppsspp_libretro`. FFmpeg is built from PPSSPP's bundled sources. The core links `libGLESv2.so.2` and `libEGL.so.1`, which the Brick has.

All three need only the system's libc, libm, libpthread, libstdc++ and libgcc_s. melonDS and DeSmuME need glibc 2.17 symbols, like the shipped cores. PPSSPP needs glibc 2.28 symbols, which matches the toolchain's sysroot and so should match the Brick. If PPSSPP fails to load with a "GLIBC_2.28 not found" error, that is the cause.

## Build problems and fixes

1. The container's `CMAKE_TOOLCHAIN_FILE` (`/opt/aarch64-nextui-linux-gnu/Toolchain.cmake`) sets `CMAKE_CXX_FLAGS` itself, so CPU flags passed as `-DCMAKE_CXX_FLAGS` were silently dropped. `build.sh` passes them in `CMAKE_C_FLAGS_RELEASE` and `CMAKE_CXX_FLAGS_RELEASE` instead. The shipped mGBA rule in `workspace/tg5040/cores/makefile` passes its flags the same dropped way, so its `-mcpu` flags probably never reach the compiler. That is worth checking separately.
2. GCC 8.3 crashed with an internal compiler error on `GPU/Software/Rasterizer.cpp` and `GPU/Software/TransformUnit.cpp`. Both crashes came from one inlined `copysign` call in `GPU/Software/GEMath.h`. `patches/ppsspp-gcc8-copysign.patch` replaces it with an equivalent sign-bit copy. `ice-test.sh` reproduces the check on single files.
3. GCC 8.3 cannot deduce the template argument of `IndependentTask` from a lambda with move-only captures in `Core/Screenshot.cpp`. `patches/ppsspp-gcc8-ctad.patch` adds a small `NewIndependentTask` helper and uses it there. The same pattern appears in `UI/`, which the libretro target does not build.

`build.sh` applies both patches before configuring PPSSPP. melonDS and DeSmuME built with no changes.

## What minarch can run today

### Pixel formats

melonDS asks for XRGB8888 and DeSmuME 2015 asks for RGB565. minarch supports both (`workspace/all/minarch/ma_environment.c`, `RETRO_ENVIRONMENT_SET_PIXEL_FORMAT`). PPSSPP's software path also uses XRGB8888.

### Hardware rendering

minarch does not support hardware rendering. Its `RETRO_ENVIRONMENT_SET_HW_RENDER` case (`ma_environment.c`, around line 388) logs the request and returns true, but it never creates a GL context or framebuffer for the core, never fills in `get_current_framebuffer` or `get_proc_address`, and never calls the core's `context_reset`. Its video callback (`ma_video.c`, `video_refresh_callback`) treats every frame as a CPU pixel buffer, so a core that hands over `RETRO_HW_FRAME_BUFFER_VALID` would have that marker read as a pixel pointer and would most likely crash. `GET_PREFERRED_HW_RENDER` is not handled at all.

This matters for PPSSPP. With its default `ppsspp_backend = auto`, PPSSPP asks for GL, believes minarch's "true", and then waits for a context that never comes. So the spike's `PSP.pak/default.cfg` sets `ppsspp_backend = none`. That makes PPSSPP fall through to its `LibretroSoftwareContext`, which runs the software GPU on the CPU and sends XRGB8888 frames through the normal video callback. This path works with minarch as it is, but software rendering on four A53 cores will be slow. The first PSP measurement therefore measures PPSSPP's worst case. A fair PSP result needs a hardware render path in minarch: a GLES context shared with minarch's own renderer, an FBO handed to the core through `get_current_framebuffer`, `get_proc_address` through EGL or SDL, a `context_reset` call after load, and drawing the FBO texture when the frame pointer is `RETRO_HW_FRAME_BUFFER_VALID`. minarch already draws with GLES, so this is new work in minarch rather than a new graphics stack.

melonDS and DeSmuME 2015 do not need hardware rendering. melonDS was built without its OpenGL renderer on purpose.

### The DS's 256x384 frame

Both DS cores default to stacking the two screens, so the frame is 256 x 384. melonDS reports an aspect ratio of 256/384, and DeSmuME 2015 reports 0, which libretro reads as width over height, also 2:3. minarch's scaler takes any frame size, so this should display as a tall picture. At 2x it is 512 x 768 on the 1024 x 768 screen, with black on both sides. Both cores also offer side by side, single-screen and hybrid layouts through their core options (`melonds_screen_layout`, `desmume_screens_layout`), which minarch's in-game options menu lists.

### Touch screen

minarch's input callback answers only the joypad and the analog sticks. It never answers `RETRO_DEVICE_POINTER` or `RETRO_DEVICE_MOUSE`, and the Brick has no stick. So neither DS core gets touch input in this spike. Games that need a tap to start or to pick menu items will not be fully playable until minarch gets a D-pad-driven cursor that it reports as a pointer.

## Files for a hand test on the Brick

minarch finds a core by path. The launcher looks for an emulator pak at `/mnt/SDCARD/Emus/tg5040/<TAG>.pak/launch.sh` (and at `.system/tg5040/paks/Emus/<TAG>.pak` for built-in ones), where `<TAG>` is the text in parentheses at the end of the ROM folder's name. Extra paks such as `Emus/tg5040/VB.pak` keep their core next to `launch.sh` and set `CORES_PATH=$(dirname "$0")`. The spike paks in `paks/` follow that layout. minarch sets the core's system directory to `/mnt/SDCARD/Bios/<TAG>` and its save directory to `/mnt/SDCARD/Saves/<TAG>`.

These are the files and where each would go. Nothing has been pushed.

| Local file | Device path |
|---|---|
| `spikes/ds-psp/paks/NDS.pak/launch.sh` | `/mnt/SDCARD/Emus/tg5040/NDS.pak/launch.sh` |
| `spikes/ds-psp/paks/NDS.pak/default.cfg` | `/mnt/SDCARD/Emus/tg5040/NDS.pak/default.cfg` |
| `spikes/ds-psp/out/melonds_libretro.so` | `/mnt/SDCARD/Emus/tg5040/NDS.pak/melonds_libretro.so` |
| `spikes/ds-psp/out/desmume2015_libretro.so` | `/mnt/SDCARD/Emus/tg5040/NDS.pak/desmume2015_libretro.so` |
| `spikes/ds-psp/paks/PSP.pak/launch.sh` | `/mnt/SDCARD/Emus/tg5040/PSP.pak/launch.sh` |
| `spikes/ds-psp/paks/PSP.pak/default.cfg` | `/mnt/SDCARD/Emus/tg5040/PSP.pak/default.cfg` |
| `spikes/ds-psp/out/ppsspp_libretro.so` | `/mnt/SDCARD/Emus/tg5040/PSP.pak/ppsspp_libretro.so` |
| `spikes/ds-psp/out/PPSSPP/` (whole folder) | `/mnt/SDCARD/Bios/PSP/PPSSPP/` |

Howard's games go in `/mnt/SDCARD/Roms/Nintendo DS (NDS)/` and `/mnt/SDCARD/Roms/Sony PlayStation Portable (PSP)/`. These are the ROM folder names the plan already proposes.

`NDS.pak` runs melonDS by default. To test DeSmuME 2015 on the same ROM folder, create `/mnt/SDCARD/Emus/tg5040/NDS.pak/core.txt` containing the single word `desmume2015`, and delete it to go back. The two cores write different save files (`.sav` and `.dsv`) in `/mnt/SDCARD/Saves/NDS/`, so they do not overwrite each other. Each core logs to `/mnt/SDCARD/.userdata/tg5040/logs/NDS-<core>.txt` (the `LOGS_PATH` folder), and PPSSPP logs to `PSP.txt` there.

The spike `default.cfg` files turn on what should be fastest: melonDS with the JIT and the threaded renderer, DeSmuME with four rasterizer threads, and PPSSPP with the software backend and a frame skip of 1. Neither DS core needs a BIOS (melonDS boots with its built-in FreeBIOS). PPSSPP needs no BIOS, only the assets folder above.

Pushes use `adb push` to those paths. If the launcher does not show the new ROM folders afterwards, `adb shell touch /tmp/launcher_reload` restarts it.

## What blocks a fair measurement

- PSP can only run with the software renderer until minarch gets a hardware render path. Its numbers will understate what PPSSPP can do on the GE8300.
- DS games cannot be touched. Most of Howard's games are menu and turn based, so menus driven by buttons can still be measured, but any screen that needs a tap will stall.
- DeSmuME 2015 has no JIT on AArch64, so its numbers are interpreter numbers.
- The launcher has no Home tile, short name or glyph for DS and PSP yet. The ROM folders should still appear in the console list, but they may look plain.

## DSperate (standalone)

melonDS ran Final Fantasy III's intro at about 57 of 60 fps with the CPU at its limit, so Howard approved trying DSperate, a standalone DS emulator that reimplements DraStic's JIT and NEON techniques. It is not a libretro core. It runs instead of minarch, from the same `NDS.pak`, when `core.txt` says `dsperate`. It built on 2026-11-05.

| Program | Source | Commit | Stripped binary | Licence |
|---|---|---|---|---|
| DSperate 3.3.0 | github.com/beebono/DSperate, tag `v3.3.0` (the latest tag) | `99618eb956c66083618395dbae9bb5cbf0241d16` (2026-11-03) | `out/dsperate`, 6,018,120 bytes (5.7 MB) with the three patches | GPL-3.0-or-later. FreeBIOS is under its own licence in `src/core/bios/LICENSE.freebios`. |

The build also uses the Khronos Vulkan headers, github.com/KhronosGroup/Vulkan-Headers at tag `v1.3.296` (`29f979ee5aa58b7b005f805ea8df7a855c39ff37`, Apache-2.0 or MIT), cloned to `src/vulkan-headers`. They are headers only. Nothing from them is linked.

### How to build it

```sh
git -C spikes/ds-psp/src clone https://github.com/beebono/DSperate dsperate
git -C spikes/ds-psp/src/dsperate checkout v3.3.0
git -C spikes/ds-psp/src clone --depth 1 --branch v1.3.296 https://github.com/KhronosGroup/Vulkan-Headers vulkan-headers
docker run --rm -v "$PWD/workspace":/root/workspace -v "$PWD/spikes/ds-psp":/root/ds-psp \
  ghcr.io/loveretro/tg5040-toolchain:latest /bin/bash -c \
  '. ~/.bashrc && export PLATFORM=tg5040 UNION_PLATFORM=tg5040 && /root/ds-psp/build.sh dsperate'
```

It builds with CMake and Ninja, with LTO, and the log goes to `dsperate-build.log` when run as in the other builds.

### Flags

CMake, `Release`, with `-DDSPERATE_TUNE="-mcpu=cortex-a53 -mtune=cortex-a53 -fomit-frame-pointer"`. DSperate adds `DSPERATE_TUNE` to every compile and to the LTO link, so the toolchain file cannot drop it, and all 168 compile commands carry it. DSperate itself adds `-O3`, LTO, `-fno-stack-protector` and `-D_FORTIFY_SOURCE=0`. The switches are JIT on (the AArch64 recompiler), NEON on, SDL frontend on, Wayland off, headless and tests off, RetroAchievements and LAN wireless built (both off in the config), the OpenGL ES 3D raster on, the Vulkan presenter and 3D raster on, and PGO off. PGO is off because DSperate's committed profiles are for GCC 10.5, 12.4 and 13.3, and its configure refuses a profile made by another compiler. libstdc++ and libgcc are linked statically, as the NextUI DSperate pak does, and `-lpthread` is forced onto the end of the link line.

The binary needs only `libSDL2-2.0.so.0`, `libdl`, `libpthread`, `libm` and `libc`, and glibc 2.27 symbols at most, so the Brick's glibc 2.33 is enough. It was compiled against the sysroot's SDL 2.26.1 headers and runs on the Brick's SDL 2.30.8 in `/usr/trimui/lib`, which `MinUI.pak/launch.sh` already puts on `LD_LIBRARY_PATH`. Nothing has to be bundled. DSperate opens ALSA, libcurl, libEGL, libGLESv2 and libvulkan with `dlopen` when it needs them, and the Brick has all five (`/usr/lib/libasound.so.2`, `libcurl.so.4`, `libEGL.so.1`, `libGLESv2.so.2`, `libvulkan.so.1`).

### Build problems and fixes

1. The sysroot's Vulkan headers are too old for DSperate's Vulkan code (no `VK_API_VERSION_1_2`, no `VkPhysicalDeviceShaderAtomicInt64Features`), and the build failed in `src/core/gpu/vk/`. `build.sh` points `DSPERATE_VULKAN_INCLUDE` at the newer Khronos headers instead. The NextUI DSperate pak builds with `DSPERATE_VULKAN=OFF`, but the Brick does have `/usr/lib/libvulkan.so.1`, so the Vulkan path is kept to be tried.
2. GCC 8 miscompiles one NEON kernel, `scale_row_grid`, which only the LCD grid and chunky filters use. `patches/dsperate-gcc8-neon-scale-row-grid.patch` falls back to the portable code on GCC 8. It comes from josegonzalez/minui-nintendo-dsperate-pak (MIT), commit `8c047207`. That pak's other GCC 8 fixes are already in DSperate 3.3.0 (`f71070e` keeps `layout.cpp` out of LTO, `f8a575f` links `stdc++fs`).
3. `patches/dsperate-stylus-tap-under-hold.patch` is a design change, not a build fix. Upstream, the touch button always touches, so binding it to A would take A away from the game. The patch makes a touch button that is also a DS button's binding touch only while the pen hold (R2) is held. A alone stays the DS's A. Releases are routed to whichever side took the press, so letting go of R2 before A does not leave the pen down.

4. `patches/dsperate-bramley-power.patch` adds the power key. See "Power key and sleep" below.

`build.sh` applies the three patches in that order before configuring, and skips any that are already applied. Without the second one, the config should use `stylus_button = +lefttrigger` (L2 touches) and give the screen swap another control.

The build's quick check ran `dsperate --version` in an arm64 Debian container and printed `DSperate v3.3.0 (99618eb-dirty)`, where "dirty" means the patches. The container's SDL has no dummy video driver, so the emulator itself has not run yet. The clean build on 2026-11-05 gave the same 310 compiler warnings as before the power patch, all in DSperate's core, and none in `src/frontend/sdl`.

### Power key and sleep

DSperate has no power key handling of its own, and nothing else on the Brick handles the key while it runs. Howard approved option (a) of the plan on 2026-11-05 (`docs/plans/ds-psp.md`, "Power and sleep for DSperate"). `patches/dsperate-bramley-power.patch` adds `src/frontend/sdl/bramley_power.cpp` and `.h` (about 370 lines with comments) and 35 lines of hooks in `main.cpp`, marked `Bramley:`. It repeats Bramley's `PWR_update()` and `PWR_sleep()` from `workspace/all/common/api.c`. The way it finds the power key comes from the NextUI DSperate pak's GPL-3.0 `overlay/minui.cpp`, with credit in the file.

- **Finding the key.** At start it opens each `/dev/input/event*` node, keeps the one that reports `KEY_POWER`, and reads it every frame without a grab, also while the pause menu is open. The log says which node it found.
- **Tap (released within a second).** DSperate pauses (audio stops), writes the auto state and the battery save, and syncs. Then, as `PWR_enterSleep()` does, it stops the game timer, sets the raw volume to 0, turns the backlight off and sends SIGSTOP to keymon, batmon and audiomon. Only a power tap wakes, as in the rest of Bramley. Waking sends SIGCONT, turns the backlight on, unmutes the amplifier at zero volume, waits 60 ms, restores the volume, resumes the game timer, reopens the audio device as `api.c` does, and unpauses. Presses in the first second after waking are ignored.
- **Deep sleep.** After the suspend timeout (30 seconds by default) it runs `$SYSTEM_PATH/bin/suspend`, unless the Brick is charging or kept awake on USB, in which case it checks again a minute later. When `bin/suspend` returns, the Brick is awake. DSperate's own lid code sees the clocks drift and closes and opens the DS lid. If `bin/suspend` fails, DSperate wakes, saves and powers off, as `api.c` does.
- **Hold for a second.** It mutes, turns the backlight off, touches `/tmp/poweroff` and quits through DSperate's normal exit, which writes the auto state and the battery save. `run.sh` syncs, and `MinUI.pak/launch.sh` runs `poweroff_next`.
- **Auto sleep.** With no input for the screen timeout (60 seconds by default), it sleeps as a tap does, unless charging or kept awake on USB. A held button or D-pad direction counts as input.
- **Settings.** `screentimeout`, `suspendTimeout` and `keepAwakeWhenUSB` come from `$SHARED_USERDATA_PATH/minuisettings.txt`, with `config.h`'s defaults. Brightness and volume go through `libmsettings.so`, opened at run time from `$SYSTEM_PATH/lib`. Without it, the power key only powers off.
- **Crash while asleep.** DSperate writes `/tmp/bramley_dsperate_asleep` before it turns anything off and removes it after waking. A SIGTERM while asleep wakes first, then saves and exits. If DSperate exits any other way with the marker present, `run.sh` runs `dsperate --bramley-wake`, which restores the backlight, volume and helpers, and then sends SIGCONT to the helpers itself in case that failed.
- **Not copied from `PWR_enterSleep()`.** The LED sleep profile, the haptic pulse and the CPU frequency change are left out, since they were not part of the approval. The plan's `auto_resume.txt` is left out too: the launcher would leave `/tmp/resume_slot.txt` behind for the next minarch game, and with `autoload = true` DSperate resumes from its auto state anyway.

### Files and device paths

Nothing has been pushed. A test needs these on top of the spike's `NDS.pak` files listed above:

| Local file | Device path |
|---|---|
| `spikes/ds-psp/paks/NDS.pak/launch.sh` (updated) | `/mnt/SDCARD/Emus/tg5040/NDS.pak/launch.sh` |
| `spikes/ds-psp/out/dsperate` | `/mnt/SDCARD/Emus/tg5040/NDS.pak/dsperate/dsperate` |
| `spikes/ds-psp/paks/NDS.pak/dsperate/run.sh` (updated for the power patch) | `/mnt/SDCARD/Emus/tg5040/NDS.pak/dsperate/run.sh` |
| `spikes/ds-psp/paks/NDS.pak/dsperate/dsperate.ini` | `/mnt/SDCARD/Emus/tg5040/NDS.pak/dsperate/dsperate.ini` |
| `spikes/ds-psp/src/dsperate/LICENSE` | `/mnt/SDCARD/Emus/tg5040/NDS.pak/dsperate/LICENSE.DSperate` |

`dsperate` must keep its execute bit after the push (`adb push` keeps the local mode).

DSperate writes these on the card when it runs:

- `/mnt/SDCARD/.userdata/tg5040/NDS-dsperate/dsperate/dsperate.ini`, seeded once from the pak's `dsperate.ini`. Delete it to seed it again.
- `/mnt/SDCARD/.userdata/tg5040/NDS-dsperate/dsperate/games/<rom name>.ini`, the per-game layout and swap.
- Battery saves in `/mnt/SDCARD/Saves/NDS/dsperate/<rom name>.sav`.
- Save states in `/mnt/SDCARD/.userdata/shared/NDS-dsperate/<GAMECODE>.auto.dss` (written on quit) and `<GAMECODE>.<slot>.dss`.
- The log in `/mnt/SDCARD/.userdata/tg5040/logs/NDS-dsperate.txt`.

### Saves

DSperate names its battery save `<rom name>.sav`, exactly as melonDS does in `/mnt/SDCARD/Saves/NDS/`. In the same folder the two would read and write the same file. That might even work, since both write a raw save chip image, but the two emulators can size a save chip differently, and a test emulator should not touch a real save. So DSperate's saves go to their own folder, `Saves/NDS/dsperate/`, and the two never meet. A melonDS save can be copied there by hand to carry a game over. DSperate's states are named by the four-letter game code, so a translated ROM and its original share state slots.

### How to switch

Create `/mnt/SDCARD/Emus/tg5040/NDS.pak/core.txt` containing the single word `dsperate`. `desmume2015` still picks DeSmuME 2015, and with no `core.txt` the pak runs melonDS as before. `launch.sh` now strips spaces and line endings from `core.txt`, so a file saved with a Windows line ending still works.

### Controls and the design

The pak's SDL mapping is the Brick profile from the NextUI DSperate pak: the Brick's pad reports itself as an Xbox 360 pad (bus 3, `045e:028e`, version `0114`, read from `/proc/bus/input/devices`), so `run.sh` exports a mapping for GUID `030000005e0400008e02000014010000`, which replaces SDL's built-in Xbox mapping. MENU is SDL's `guide`, the modifier. The config sets `face_fix = off`, because TrimUI puts Xbox key codes under Nintendo labels and DSperate's automatic check would otherwise swap A and B.

| Design | DSperate config | Matches |
|---|---|---|
| Stacked by default, 2x | `layout = vertical`, `integer_scale = under` | Yes, 512x768 |
| Layout remembered per game | DSperate writes `video.layout` to `games/<rom>.ini` | Yes |
| L2 swaps the screens, remembered | `screen_swap = +lefttrigger`, stored as `video.screen` | Yes |
| MENU+Left/Right cycles layouts | `layout_next = mod+dpright`, `layout_prev = mod+dpleft`, `layout_cycle = vertical,horizontal,single,dominant_h` | Yes |
| Tap MENU opens the menu | `pause.alt = mod` | Yes, DSperate's own pause menu |
| Hold R2, D-pad moves the pen | `stylus_hold = +righttrigger` | Yes |
| R2+A touches, holding A drags | `stylus_button = b` with the spike patch | Yes, only with the patch |
| MENU+START moves the small screen | `pip_corner_next = mod+start` | Partly, see below |
| fps counter | `fps = true`, MENU+Y toggles it | Not in the design. On for measuring |

The fps counter sits in a corner of the main screen and counts presented frames. `fps = true` in `[video]` turns it on and `fps = mod+x` (MENU and the Brick's Y) toggles it. `run.sh` also sets `DS_FPS=1`, which writes a line to the log every 60 frames with the fps, the share of full speed, and the emulation, present and wait times, so a run can be read afterwards. The config has `frameskip = 0`, so the counter shows the real speed. spruceOS and the NextUI pak ship `frameskip = 1` (adaptive), which is the setting to try if the game is close to full speed and the audio crackles.

Differences from the design:

- Large plus small is DSperate's `dominant_h` layout. On 1024x768 it gives exactly the design's 3x (768x576) plus 1x (256x192), but the small screen sits bottom-aligned with the large one, at x 768 and y 480, not top right, and it cannot be moved. MENU+START moves the inset of DSperate's separate `pip` layout, which is not in the cycle. Its corners are the four corners, not top right, centre and bottom right.
- In one-screen mode, holding R2 does not bring up the touch screen. L2 swaps to it instead.
- The pen has no acceleration. It moves at a constant `stylus_speed`, 4 DS pixels a frame (the whole width in about one second), so a single tap moves it 4 pixels, not 1.
- The pen hides 30 frames (half a second) after it stops moving, not after R2 is let go. Since the D-pad moves it only while R2 is held, the effect is close.
- No one-second label appears when the layout changes or the screens swap.
- The empty areas are black, as in the design, but DSperate's pause menu is its own, not Bramley's, and it does not follow the launcher's look.
- "Use for all DS games" is not a menu item. The per-game choice is a file, and the global default is `layout` in `dsperate.ini`.
- MENU+SELECT quits straight away, an extra safety exit for the spike. The pause menu's Quit item also leaves.
- Save states, fast forward and screenshots have no buttons, to keep the chords free. The pause menu still saves and loads states.

### Risks to check on the device

- **Display path.** On Allwinner chips DSperate's automatic `sink` tries the display engine (`/dev/disp`) first. That path was written for the Miyoo A30, and on the A133P it is unverified. The `video:` line in the log names the path that opened. If the picture is wrong, set `sink = renderer` (or `sink = fbdev`) under `[video]` in the seeded `dsperate.ini`.
- **Power button and sleep.** Upstream DSperate does not read the power button, and NextUI's sleep lives in the launcher and minarch (`PWR_update` in `api.c`), not in a daemon. While DSperate runs, a short press of power will probably do nothing, and the Brick will not sleep on its own. The NextUI DSperate pak adds sleep and safe shutdown through its own patch, which is not in this build. Leave through the pause menu's Quit or MENU+SELECT. A forced shutdown by holding power loses unsaved progress since the last battery save, though DSperate writes battery saves shortly after the game writes them.
- **Exit.** DSperate exits normally from Quit, `run.sh` syncs, `launch.sh` returns, and `MinUI.pak/launch.sh` starts the launcher again. DSperate also flushes the battery save and the auto state on SIGTERM. The launcher is not running while DSperate runs, so nothing has to be stopped, and nothing needs `kill`.
- **Volume and brightness.** `keymon` handles the volume keys and MENU+volume on its own, so they should still work. MENU+volume is not one of DSperate's chords.
- **Input.** The mapping body was verified on the Brick by the NextUI pak's author. The GUID here was worked out from the evdev ids and SDL's rules (SDL ignores the name checksum when matching), not read from a running program. The log's `controller:` line shows that a pad opened. If the buttons do not answer at all, that GUID is the first thing to check.
- **The pause menu's scale.** On the display-engine path DSperate draws its menu on an overlay layer, and its size on a 1024x768 panel is unverified.
- **GPU 3D.** `gpu3d` is off, so the 3D raster runs on the CPU, as in melonDS. If DSperate is close but short of full speed, `gpu3d = true` under `[video]` moves the 3D layer to the GE8300 through Vulkan, or through OpenGL ES 3.1 if Vulkan fails. That is a second measurement, not the first.
- **Real-time threads.** DSperate asks for real-time scheduling (`realtime = rr`) for its threads by default. It runs as root on the Brick, so this should work. With nothing else running, it is unlikely to starve the system, but a stuck emulator could be harder to leave.
