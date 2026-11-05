# Nintendo DS and PSP support

Draft, 2026-11-04. Howard has 11 DS games and 1 PSP game (Ultraman Fighting Evolution 0) that Bramley cannot run, because its card ships no DS or PSP emulator. This plan is for deciding how to add them. Nothing is built yet: new emulator cores need Howard's OK (CLAUDE.md, hard rules).

## What the Brick has to work with

- Chip: Allwinner A133P, four Cortex-A53 cores, PowerVR GE8300 GPU, 1 GB of RAM. The Brick and Brick Hammer share it.
- Screen: 1024 x 768, 4:3. The DS has two 256 x 192 screens; the PSP is 480 x 272, 16:9.
- Controls: a D-pad, four face buttons, four shoulder buttons. No analog stick and no touch screen.
- Community reports put DS as playable on this chip with DraStic, and PSP as "many games playable, not all at full speed", game by game. No measured numbers turned up, so step 1 below measures them.

## Options

### Nintendo DS

| Option | Speed on the A53 (to measure) | Licence | Fits Bramley |
|---|---|---|---|
| A. DraStic, standalone | The usual choice on these chips; reported as the fastest | Closed source and proprietary. Free since March 2024, but no licence text, and no stated permission to redistribute | Cannot ship on the card without the developer's written permission. Its own menu, not Bramley's in-game menu |
| B. melonDS, libretro core | Accurate but heavy; likely too slow for many games on an A53 | GPL-3.0 | Ships like the other cores, uses Bramley's in-game menu, save states and Activity |
| C. DeSmuME 2015, libretro core | Older and lighter than melonDS; some 2D games may run | GPL-2.0 | Same as B |

A middle path for DraStic: "bring your own". Bramley ships only a launch script and button layout, and the user copies the DraStic binary onto the card themselves, as some other custom firmwares do. That keeps closed code off Bramley's card, but the experience is rougher and Bramley's in-game menu would not apply.

The DS's two screens on one 4:3 display: side by side shrinks both, stacked fits 256 x 384 into 768 rows at 2x (with black sides), and a swap button lets one screen fill the display. The touch screen needs a cursor driven by the D-pad or a shoulder button. Howard's games (Super Robot Wars K and W, Ace Attorney 4, the Final Fantasy set, Kirby Super Star Ultra) lean on menus and turns more than on touch, which helps.

### PSP

| Option | Speed | Licence | Fits Bramley |
|---|---|---|---|
| D. PPSSPP, libretro core | Game by game; a muOS forum guide calls PSP "a nice to have" on these chips | GPL-2.0 or later | Ships like the other cores, with Bramley's in-game menu. Uses OpenGL ES (the GE8300 also has Vulkan) |
| E. PPSSPP, standalone | Similar, sometimes faster, with its own settings for this class of chip | GPL-2.0 or later | Its own menu, not Bramley's |

With no analog stick, games that steer with the PSP's stick are mapped to the D-pad, which suits fighting games like Ultraman better than 3D action games. The 16:9 picture letterboxes on the 4:3 screen at about 2x.

## Recommendation

1. Measure first (a spike in `spikes/ds-psp`, nothing shipped). Build melonDS, DeSmuME 2015 and PPSSPP as libretro cores for tg5040 in the toolchain container, push them to the Brick by hand (saying which files and paths first), and record frame rates on Howard's own games: Super Robot Wars W, Ace Attorney 4, Kirby Super Star Ultra and Final Fantasy III for DS, and Ultraman for PSP. About a day.
2. PSP with PPSSPP as a libretro core (option D), if the measurements hold up. It is GPL like the cores Bramley already ships, and it gets the in-game menu, save states and Activity for free.
3. DS by the measurements: a libretro core (B or C) if one is fast enough for Howard's games; otherwise ask DraStic's developer for permission to bundle, and fall back to "bring your own DraStic" if there is no answer.

## Work once a choice is made

- Pin each core's source commit in `workspace/tg5040/cores/makefile`, add its licence to `Licenses/` and `CORES-SOURCE.md`, and check it against `docs/plans/cores-licenses.md`. Clear `extras-also.txt` because the card's extras change.
- Emulator Paks: `Emus/NDS.pak` and `Emus/PSP.pak` with the default settings that measured best (DS screen layout, PSP frame skip and renderer).
- Rom folders on the card: `Nintendo DS (NDS)` and `Sony PlayStation Portable (PSP)`, with short names for Home ("DS", "PSP"), a tile colour and a symbol from `tools/console-glyphs.py`.
- BIOS: none needed for melonDS in its built-in mode or for PPSSPP. DraStic needs none either.
- Card size: the cores add roughly 10 to 30 MB to the card zip and the update.
- Test on Howard's Brick with his games, then ship in the next 0.9.x release.

## Decisions for Howard

1. Approve building and testing the three libretro cores on the Brick (step 1). The emulator cores are on the list that needs your OK.
2. DraStic: ship nothing of it, support "bring your own", or ask its developer for permission to bundle.
3. Whether PSP is worth adding at all, given no analog stick and game-by-game speed.

### Answers (2026-11-05)

1. Approved: build melonDS, DeSmuME 2015 and PPSSPP as libretro cores in `spikes/ds-psp` and measure them on the Brick. Every push names its files and paths first.
2. DraStic is decided after measuring. If a libretro core runs Howard's games well enough, Bramley skips DraStic. If not, ask its developer for permission to bundle, and support "bring your own" meanwhile.
3. PSP is wanted. First, research how TrimUI's own OS and the other systems on the Brick handle the PSP's analog stick, and how they handle the DS's two screens (`docs/plans/ds-psp-research.md`).

## Measurements (2026-11-05)

Read from minarch's Debug HUD on Howard's Brick, through screen captures over adb. The frame rate counter is the reliable number. The HUD's `D:` counter and frame times are wrong for melonDS, because they count a loop over 18 ms as a drop even while 60 frames a second arrive.

| Game | Emulator | Scene | Frame rate | Notes |
|---|---|---|---|---|
| Super Robot Wars W | melonDS (JIT, threaded renderer) | Title screen | 60.2 of 59.9 | CPU at 2 GHz and fully loaded |
| Kirby Super Star Ultra | melonDS | Intro scene with dialogue | 59.3 to 60.0 | CPU fully loaded, GPU idle |
| Final Fantasy III | melonDS | Opening movie | 57.0 to 57.8 | The sound buffer ran empty and Howard heard the audio struggle. Setting Audio Resampling Quality to its lowest helped a little, not enough. |
| Ultraman Fighting Evolution 0 | PPSSPP, software renderer, frame skip 1 | A fight | Shown 60.2, but each frame takes 43 to 47 ms (peak 65), so the game runs at roughly 20 to 25 frames a second | Howard: "it feels like slow motion". The sound buffer ran empty. CPU at 279 to 284 percent across four cores, at 1.8 GHz and 63 to 64°C (probably throttled by heat). GPU idle. PPSSPP logged "Running slow". |

| Final Fantasy III | DSperate 3.3.0 (standalone, JIT, CPU 3D) | Opening movie | 59.8 of 59.8 for 25 seconds | 3.4 to 5.6 ms per frame, read from DSperate's own once-a-second log. The sound queue stayed between 1 and 6 frames. |
| Final Fantasy III | DSperate | Name entry | 59.8 of 59.8 for all 91 logged seconds | 6.1 to 6.7 ms per frame. 50°C at 2 GHz. Howard tested the pen: hold R2 and the D-pad moves it, A taps, and it works. |

| Final Fantasy III | DSperate | First battle, 3D, three goblins | 59.8 of 59.8 in every one of 206 logged seconds | 6.9 to 7.3 ms per frame, 8.1 ms at worst. 51°C at 2 GHz. Howard: the sound is clean. |

DSperate makes a frame about four times faster than melonDS on the same scene and leaves about half of each frame idle even in battle.

### DS decision (2026-11-05)

Howard chose DSperate (github.com/beebono/DSperate, GPL-3.0, tag v3.3.0, commit `99618eb956c66083618395dbae9bb5cbf0241d16`) as Bramley's DS emulator. melonDS and DeSmuME 2015 drop out, and DraStic is not needed, so its licence question is closed.

DSperate is standalone, so Bramley wraps it rather than running it in minarch. Work before it ships:

- **Power and sleep.** DSperate does not read the power button. The wrapper must give it sleep and a safe shutdown, as minarch has. The NextUI DSperate pak has a patch for this to study first.
- **Pause menu.** DSperate's own menu appears today. Bramley's in-game menu (save states, quit, settings) should replace it or look like it.
- **Screen design gaps** (from the spike's README): the small screen in large plus small cannot move; holding R2 in one-screen mode does not show the touch screen; the pen has no acceleration and moves 4 pixels a tap; no layout label; no "Use for all DS games" item. Each needs a patch to DSperate or a decision to accept the difference.
- **Patches to carry.** `dsperate-stylus-tap-under-hold.patch` (A taps while R2 is held) and the GCC 8 NEON fix. Offer the stylus patch upstream.
- **Saves.** Battery saves go to `Saves/NDS/` (the spike used `Saves/NDS/dsperate/` only to keep clear of melonDS). Save states are keyed by game code. Activity, Continue Playing and the save-state thumbnails must learn DSperate's file names.
- **Display path.** It opened fbdev with the Mali driver at 1024x768, which worked. Keep it.
- **Card and licence.** Pin the commit in the cores makefile or a new standalone build rule, add GPL-3.0 and the source link to `Licenses/` and `CORES-SOURCE.md`.

The shown frame rate means little for PPSSPP, because it emulates on its own thread and minarch repeats the latest picture at 60 a second. The frame time and the sound buffer show the real speed.

melonDS has no more speed to give: its JIT and every JIT option are on, and it draws on its own thread. The CPU is the limit. Next step, approved by Howard: build DSperate and compare it on Final Fantasy III.

PSP is not playable with the software renderer. The fix is a hardware render path in minarch, so PPSSPP can draw with the GE8300, which sits idle today (see `spikes/ds-psp/README.md`, "Hardware rendering"). That is new work in minarch, estimated at a few days, and waits for Howard's decision after the DSperate comparison.

## DS screens and touch (decided 2026-11-05)

Howard and Claude settled this design in a grilling round before measuring. Building waits until the measurements choose the DS emulator. The measurements go ahead now with each core's own screen settings, because the layout barely changes speed.

### Who owns the screens

- Bramley defines one behaviour for the DS screens, the buttons and the pen. With melonDS or DeSmuME, the core sends its plain stacked 256x384 picture and minarch draws the layouts.
- Nothing the emulator offers is lost. Bramley's layer is on by default, and a per-game switch in Options, then Emulator, "Screens: Bramley / Emulator", hands the screens, buttons and pen fully back to the core. Every core option stays listed on that page either way.
- If DSperate or DraStic wins the measurements, Bramley sets up their config files to match this design as closely as they allow, and the plan lists each difference (for example, no one-second label, or DSperate tapping with L2 instead of A).

### Layouts

All four fit 1024x768 at whole-number scales:

| Layout | Size | Notes |
|---|---|---|
| Stacked | 2x, 512x768 | The default for every game. Black at the sides. |
| Side by side | 2x, 1024x384 | Kept for now. Check it in use, then ask Howard whether to delete it. |
| One screen | 4x, 1024x768 | Fills the display. |
| Large plus small | 3x (768x576) plus 1x (256x192) | The small screen sits in the 256-pixel column. MENU+START moves it between top right (the default), centre and bottom right, remembered per game. |

The empty areas are black. The wallpaper or a blurred copy of the game can be tried after the measurements, if there is GPU time to spare.

### Buttons

- L2 swaps the two screens.
- MENU plus Left or Right cycles the layouts. A tap of MENU alone still opens the in-game menu.
- Holding R2 turns the D-pad into the pen. While R2 is held, A touches and holding A drags. Letting go of R2 gives the controls back to the game, so there is no hidden mode.
- In one-screen mode, holding R2 shows the touch screen until R2 is let go.
- The pen shows only while R2 is held and for half a second after.
- The pen accelerates: a tap of the D-pad moves it one DS pixel, and holding a direction speeds it up to cross the screen in about half a second.

### Memory and feedback

- Each game remembers its layout, swap state and small-screen position. The in-game menu has "Use for all DS games".
- Swapping or changing the layout shows a one-second label at the top ("Stacked", "Large + small"), in the toast style from `docs/designs/agent-pager.md`.

### Known gaps in the first version

- No microphone. A game that asks the player to blow into it will stall.
- No rotated layout for games held sideways, such as Hotel Dusk and Brain Age. None of Howard's games need it.

## Power and sleep for DSperate (approved 2026-11-05, option a)

Written 2026-11-05. This touches power and sleep, so nothing was built until Howard approved it (CLAUDE.md, "Hard rules"). Howard approved option (a) on 2026-11-05, exactly the three points under "Approval Howard needs to give". It is built (see "Implementation" below) and has not gone to the device.

### What Bramley does today

Power handling lives inside each SDL program, not in a daemon. The launcher and minarch call `PWR_update()` every frame, and nothing else on the system acts on the power key.

- **Who reads the key.** The Brick's power key arrives through SDL as a keyboard event with scancode 102 (`CODE_POWER`, `workspace/tg5040/platform/platform.h:72`; the kernel code is `KEY_POWER` 116, line 43). `PAD_poll()` turns it into `BTN_POWER` (`workspace/all/common/api.c:3525-3529`), and `BTN_SLEEP` and `BTN_WAKE` are both `BTN_POWER` on tg5040 (`platform.h:131-132`). keymon opens `/dev/input/event0` to `event4` without grabbing them (`workspace/tg5040/keymon/keymon.c:112-116`), but it only handles volume, brightness, colour temperature, MENU and the headphone jack (lines 155-171). It ignores the power key. So when the launcher and minarch are not running, nothing handles power.
- **Tap and hold.** In `PWR_update()` (`api.c:4146`), pressing power records the time (`api.c:4187-4198`). Releasing it within a second sleeps (`api.c:4207`). Holding it for a second runs the `before_sleep` callback, stops the game timer and calls `PWR_powerOff()` (`api.c:4179-4185`). A press within a second of waking is ignored (`api.c:4189`).
- **Auto sleep.** With no input for the screen timeout (default 60 seconds, `workspace/all/common/config.h:230`), `PWR_update()` sleeps the same way, unless the Brick is charging, on HDMI, or set to stay awake on USB (`api.c:4200-4206`, `PWR_preventAutosleep()` at `api.c:4566`).
- **What minarch does before sleep.** `Menu_beforeSleep()` writes the battery save and the RTC, saves the auto state, and writes `auto_resume.txt` (`workspace/all/minarch/ma_menu.c:225-230`). `Menu_afterSleep()` removes `auto_resume.txt` and restores the overclock (`ma_menu.c:231-234`). So a game in minarch is fully saved before the screen goes dark.
- **Sleep is two stages.** `PWR_sleep()` (`api.c:4514-4530`) is "hybrid sleep":
  1. `PWR_enterSleep()` (`api.c:4395-4427`) pauses audio (`SND_pauseAudio`), sets the LED sleep profile, sets the raw volume to zero, pulses the rumble motor, turns the backlight off (`PLAT_enableBacklight(0)` sets raw brightness 0, `workspace/tg5040/platform/platform.c:225-233`), sends SIGSTOP to keymon, batmon and audiomon, and calls `sync()`. The CPU stays on and its governor is not changed.
  2. `PWR_waitForWake()` (`api.c:4469-4512`) then loops on the calling thread, polling for a power release every 200 ms. The game is frozen because its main loop is stuck in this call. After the suspend timeout (default 30 seconds, `config.h:231`), unless the Brick is charging or kept awake on USB, it calls `PWR_deepSleep()` (`api.c:4532-4556`), which runs `skeleton/SYSTEM/tg5040/bin/suspend`. That script runs the `pre-sleep.d` hooks, stores the ALSA mixer state, stops Bluetooth and Wi-Fi if they run, writes `mem` to `/sys/power/state` (real suspend to RAM, five tries), and restarts them on wake. If deep sleep fails, the Brick powers off (`api.c:4499-4508`).
  3. `PWR_exitSleep()` (`api.c:4429-4467`) restores the LEDs, sends SIGCONT to the three daemons, turns the backlight on, raises the volume after a 60 ms settle (a Bramley fix for the amplifier click), and reopens audio with `SND_resetAudio`.
- **Power off.** `PWR_powerOff()` (`api.c:4337-4393`) draws the Bramley "Shutting down" screen, sends SIGTERM to keymon, batmon and audiomon, and calls `PLAT_powerOff()` (`platform.c:235-256`), which mutes, turns off the backlight, clears the framebuffer, touches `/tmp/poweroff`, syncs and exits. `MinUI.pak/launch.sh` sees `/tmp/poweroff` after the program returns and runs `poweroff_next` (`skeleton/SYSTEM/tg5040/paks/MinUI.pak/launch.sh:185-211`). `poweroff_next` sends SIGTERM to every process, waits only 2 seconds, then sends SIGKILL (`workspace/tg5040/poweroff_next/poweroff_next.c:85-96`), syncs, unmounts the card and cuts power through the AXP2202.
- **A SIGTERM to an SDL program** becomes `SDL_QUIT`, and NextUI's `PAD_poll()` answers that with `PWR_powerOff(0)` (`api.c:3793-3796`). This is why CLAUDE.md forbids `kill` on `launcher.elf`. DSperate sets its own SIGTERM handler, so this does not apply to it (below).
- **The launcher already has a precedent** for owning the power key itself: while music plays it hides the power edges from `PWR_update()` and handles tap and hold on its own (`workspace/all/launcher/launcher.c:1757-1785`).

### What DSperate offers (v3.3.0, `spikes/ds-psp/src/dsperate`)

- **No power key.** Nothing in `src/frontend/sdl` reads the power key or SDL's power scancode.
- **SIGTERM and SIGINT are safe.** `on_signal` sets a flag (`src/frontend/sdl/main.cpp:918-920`, installed at lines 1592-1593). The main loop checks it every frame (`main.cpp:3639`) and on the way out writes the auto state, then the battery save (`autosave_now(); flush_save();`, `main.cpp:4387-4389`). With `autosave = true` in our `dsperate.ini`, a SIGTERM gives the same result as Quit.
- **Battery saves are written about a second after the game stops writing** (60 quiet frames, `main.cpp:4346-4347`), with a write-then-rename (`main.cpp:354-366`). There is no `fsync`, so the data can sit in the page cache until something syncs.
- **No outside pause.** There is no signal or socket to pause it. The internal pause (`set_paused`, `main.cpp:2953-2959`) pauses SDL audio, clears its queue and flushes the save, but only the pause menu calls it.
- **It survives being frozen.** The frame pacer caps its debt at one frame (`src/frontend/sdl/pacer.h:70-75`), so after SIGSTOP and SIGCONT it carries on at normal speed with no burst of catch-up frames.
- **It notices a real suspend.** `Lid::poll()` compares `CLOCK_BOOTTIME` with `CLOCK_MONOTONIC` every frame. If they drift more than a second apart, the host slept, and DSperate closes the DS lid for 60 frames, then opens it and resyncs the DS clock (`src/frontend/sdl/lid.cpp:75-93`, `main.cpp:3820-3827`). A SIGSTOP alone does not change that drift, so only a real suspend causes the lid pulse. A game that sleeps on lid close (most do) will sleep and wake once, which is what a real DS does. The Brick has no lid switch, so the evdev path stays unused.

### What others do on the Brick

There are two working patterns. The NextUI DSperate pak handles power inside the emulator. Every other pak, and spruceOS, uses a separate watcher process.

| Project | Where power is read | Sleep | Shutdown | Saves before sleep or shutdown |
|---|---|---|---|---|
| josegonzalez/minui-nintendo-dsperate-pak, commit `8c047207` | Inside DSperate. `overlay/minui.cpp` finds the evdev node that reports `KEY_POWER` and reads it without a grab (lines 65-101). It polls every frame, also with the pause menu open. SDL is not used, because the power key sits on its own node that SDL does not deliver (`TECHNICAL.md` 150-158). | A release before 1 second sleeps. It pauses the emulator in-process with `set_paused(true)` (no SIGSTOP), writes the auto state and `auto_resume.txt`, stops the game timer, mutes through `/sys/class/speaker/mute`, sets the backlight to 0 through `/dev/disp` ioctl `0x102`, then polls the key every 200 ms. After 2 minutes it writes `mem` to `/sys/power/state` itself, skipping NextUI's `bin/suspend`, so Wi-Fi and Bluetooth are not stopped first (`minui.cpp` 232-287). On wake it restores everything and swallows the wake press (264-278). | A 1-second hold touches `/tmp/poweroff` and asks DSperate to quit, so its normal exit writes the auto state and battery save. NextUI's `MinUI.pak/launch.sh` then runs `poweroff_next` (patch `0003-minui-integration.patch` 455-461). | Yes, both. |
| ben16w/minui-power-control (used by josegonzalez/minui-nintendo-ds-pak for DraStic and both PSP paks, pinned at 1.1.0) | A Go watcher on the hardcoded `/dev/input/event1`, code 116 (`src/button-handler.go` 13-34). | A release before 2 seconds sends SIGSTOP to the emulator, runs NextUI's `bin/suspend` straight away, then SIGCONT (`bin/suspend` 13-42). | A 2-second hold sends SIGTERM without waiting, syncs, touches `/tmp/poweroff` (`bin/shutdown` 28-80). | No. Its README says unsaved progress is lost on shutdown. Issue #11 reports DraStic sometimes freezing after waking from deep sleep, suspected to be the SIGSTOP and SIGCONT pair. Issue #4: the screen stays off after wake if brightness was 0. |
| spruceOS, commit `00a9439f` (ships DSperate on the Brick) | A system-wide `power_button_watchdog.sh` that grabs `/dev/input/event1` exclusively with `getevent -exclusive` (line 101). The Brick's node is set in `spruce/scripts/platform/tina-stock/Brick.cfg` 111-114. | Real suspend to RAM at once, with no SIGSTOP and no save. It mutes, unloads Wi-Fi, and sets an RTC alarm (default 15 minutes) that powers off safely if it fires (`sleep_helper.sh`, `spruce/scripts/platform/a133p.sh` 40-60). | `save_poweroff.sh` sends SIGCONT then SIGTERM to every emulator, DSperate included, and waits up to about 10 seconds before SIGKILL (lines 144-220). | On shutdown only. Its DSperate profiles set `autosave = true`, as ours does, so SIGTERM gives a battery save and an auto state. |
| beebono/DSperate v3.3.0 | Nothing. No issues or PRs about power or sleep. | None of its own, but it copes with an outside suspend (the lid pulse above). | Saves on SIGTERM and SIGINT. | See "What DSperate offers". |

Licences: the DSperate pak is MIT at the top level, but `overlay/minui.cpp` and `minui.h` carry `SPDX-License-Identifier: GPL-3.0-or-later`, the same licence as DSperate. Code ported from them goes into our DSperate patch, which is GPL-3.0 anyway, with a credit line.

### Options

All three keep the existing tap and hold timings (release before 1 second sleeps, 1-second hold shuts down), keep the shutdown path through `/tmp/poweroff` and `poweroff_next`, and change nothing in `api.c`, `poweroff_next`, `bin/suspend` or the drivers.

**(a) Patch DSperate to handle power itself, following Bramley's sleep sequence.** A new file in the DSperate frontend (`bramley_power.cpp`, carried as `patches/dsperate-bramley-power.patch`), modelled on the pak's `minui.cpp`, plus a few hooks in `main.cpp`.

- Reads `KEY_POWER` from the evdev node that reports it, found by capability, without a grab, every frame, including in the pause menu.
- Tap: `set_paused(true)` (pauses SDL audio, clears the queue, flushes the battery save), `autosave_now()`, write `auto_resume.txt`, `sync()`. Then the same steps as `PWR_enterSleep()`: stop the game timer, raw volume 0, backlight 0 (through libmsettings, as keymon already links it), SIGSTOP keymon, batmon and audiomon. Wait in a 200 ms loop for the power key. After the suspend timeout from the user's settings (unless charging or kept awake on USB), run `$SYSTEM_PATH/bin/suspend`, so the pre-sleep hooks run and Wi-Fi and Bluetooth stop as they do for minarch. If that fails, take the shutdown path, which saves. On wake, mirror `PWR_exitSleep()`: SIGCONT the daemons, backlight on, amplifier on at zero volume, 60 ms settle, then the volume, game timer resumed, `set_paused(false)`, and ignore power for a second.
- Hold: touch `/tmp/poweroff` and request DSperate's normal quit. It writes the auto state and battery save on the way out, `run.sh` syncs, and `MinUI.pak/launch.sh` runs `poweroff_next` without starting the launcher.
- Auto sleep: count time since the last SDL input and sleep after the screen timeout, with the same exceptions as `PWR_preventAutosleep()`.
- Saves: the best of the three. The game is paused and saved before the screen goes dark, as in minarch. Nothing is frozen from outside.
- Audio pops: low. Audio is paused in-process before the mute, and wake uses Bramley's amplifier order. One thing to test: NextUI reopens audio after wake "otherwise it doesn't come back on sometimes" (`api.c:4458-4466`). DSperate may need the same reopen after a deep sleep.
- Battery: the same as minarch. The screen-off stage polls every 200 ms, then real suspend to RAM.
- Effort: about 1.5 days including device tests. The pak's code is proven on the Brick and covers most of it. It is one more patch to carry on each DSperate update. It is DSperate only, and it repeats Bramley's sleep sequence in C++, so a later change to `PWR_enterSleep()` must be copied by hand.

**(b) A small Bramley helper, `pwrwatch`, started by `run.sh` beside any standalone emulator.** A C program in `workspace/tg5040/pwrwatch/`, linking libmsettings and the config code, given the emulator's PID.

- Tap: mute and backlight off, SIGSTOP the emulator, then Bramley's sequence and `bin/suspend` after the timeout, then SIGCONT on wake.
- Hold: SIGCONT then SIGTERM to the emulator, wait for it to exit (up to 10 seconds, as spruceOS does, because `poweroff_next` allows only 2), sync, touch `/tmp/poweroff`.
- Saves: shutdown is safe for DSperate. Sleep is weaker than (a): DSperate cannot be asked to write its auto state without quitting, so a battery that dies during deep sleep loses play since the last in-game save. The battery save itself is safe, since DSperate writes it within a second of the game and the helper syncs before suspending.
- Audio pops: medium. SIGSTOP leaves the ALSA device running with nobody feeding it, so it underruns. Muting first hides that, but SDL has to recover the stream on SIGCONT, which needs testing.
- Risk: the freeze after deep sleep reported with this exact SIGSTOP pattern (minui-power-control issue #11).
- Effort: about 1.5 days. It can be reused for any future standalone emulator, but none is planned: PSP is going through minarch.

**(c) Teach keymon to handle power while a standalone emulator runs.** keymon already runs all the time and reads `event0` to `event4` without a grab (`keymon.c:112-116`). It would act on `KEY_POWER` only while a flag file such as `/tmp/bramley_standalone_pid` exists, otherwise doing what (b) does.

- Saves, audio, battery and risks: the same as (b).
- Extra risks: keymon is stopped by `PWR_enterSleep()` and the launcher, so a stale flag file after a crash could make keymon and the launcher both act on one press. keymon also drops input after any gap of more than a second (`keymon.c:143`), which needs care around wake. It changes a NextUI daemon every program depends on.
- Effort: about 1.5 days, with less reuse of known-good code than (a).

### Recommendation

Option (a). It is the only option that saves the game before the screen goes dark. It never freezes DSperate from outside, which avoids the reported freeze on wake. The NextUI DSperate pak has already proven it on the Brick. The only advantage of (b) is reuse, and Bramley has no second standalone emulator planned. If one comes later, (b) can be built then, and DSperate keeps its own handling.

Order of work once approved:

1. Write the patch in `spikes/ds-psp/patches/`, build with `spikes/ds-psp/build.sh`, and test on the Mac in the arm64 container as far as it goes. Sleep itself can only be tested on the device.
2. On the device, before any push, name the files and paths. The expected push is two files into the spike pak already on the card: the rebuilt `dsperate` binary and possibly `run.sh` in `Emus/tg5040/NDS.pak/dsperate/`.
3. Device checks: tap sleeps and wakes with no pop. A wait past the suspend timeout reaches real suspend, and the game resumes with the lid pulse. A 1-second hold leaves `.auto.dss` and a fresh `.sav`, then powers off. Auto sleep after the screen timeout. Charging blocks auto sleep. Audio returns after deep sleep. Wi-Fi comes back after wake. Battery drop over an hour asleep.

### Approval Howard needs to give

Howard, please approve or change these:

1. Option (a): a Bramley patch to DSperate that reads the power key and does sleep and shutdown inside DSperate, ported from the GPL-3.0 code in josegonzalez/minui-nintendo-dsperate-pak, with credit.
2. That patch may mute the speaker, set the backlight, SIGSTOP and SIGCONT keymon, batmon and audiomon as `PWR_enterSleep()` does, run `$SYSTEM_PATH/bin/suspend` (real suspend to RAM, which stops and restarts Wi-Fi and Bluetooth through the existing script), and power off by touching `/tmp/poweroff` for the existing `poweroff_next`. No change to `api.c`, `poweroff_next`, `bin/suspend`, keymon or any driver.
3. Device testing with adb, pushing only the rebuilt DSperate files into the spike's `NDS.pak` after naming them.

### Implementation (2026-11-05)

`spikes/ds-psp/patches/dsperate-bramley-power.patch`, applied by `build.sh` after the two existing DSperate patches, adds `src/frontend/sdl/bramley_power.cpp` and `.h` and a few `Bramley:` hooks in `main.cpp`. `spikes/ds-psp/README.md`, "Power key and sleep", describes each behaviour. Nothing in `api.c`, `poweroff_next`, `bin/suspend`, keymon or any driver changed. It builds with no new compiler warnings.

How it differs from the plan above:

- The power key is read from its evdev node, found by capability, as the NextUI DSperate pak does. SDL's scancode 102 is not used, because that pak reports that SDL does not deliver the key to DSperate on the Brick.
- Only a power tap wakes, as in `PLAT_shouldWake()` on tg5040. Other buttons do not wake it.
- The LED sleep profile, the haptic pulse, the CPU frequency change and `auto_resume.txt` are left out. The first three were not in the approval. `auto_resume.txt` would make the launcher leave `/tmp/resume_slot.txt` behind for the next minarch game, and DSperate already resumes from its auto state (`autoload = true`).
- Brightness and volume go through `libmsettings.so`, opened at run time, so the build needs nothing from Bramley.
- If DSperate exits while asleep, `run.sh` runs `dsperate --bramley-wake`, which turns the backlight, volume and helpers back on.

### Device test checklist

Before the push, name the files and paths (`spikes/ds-psp/README.md`, "Files and device paths"). The push is `spikes/ds-psp/out/dsperate` and `spikes/ds-psp/paks/NDS.pak/dsperate/run.sh` into `/mnt/SDCARD/Emus/tg5040/NDS.pak/dsperate/`. Watch `/mnt/SDCARD/.userdata/tg5040/logs/NDS-dsperate.txt` for the `bramley power:` lines.

- [x] The log names the power key's node at start ("power key on /dev/input/eventN").
- [x] Tap sleeps: the screen goes dark with no pop, and the log shows the state and save written first. (2026-11-05, plugged in: the auto state was saved, then "sleeping" and backlight 0.)
- [x] A power tap wakes (2026-11-05: backlight restored, audio reopened, back to 59.8 fps in a second, helpers running, marker removed; the pause-menu case is not tested yet): the screen comes back, the sound returns with no click, and the game carries on where it was. A tap with the pause menu open sleeps and wakes back into the menu.
- [ ] A second tap within a second of waking is ignored.
- [ ] Brightness and volume keys work after waking (keymon was resumed).
- [ ] Deep sleep (deferred by Howard, needs the cable out): wait more than 30 seconds asleep. The log shows "deep sleep through .../bin/suspend". A power tap wakes the Brick, the game shows the lid pulse, and the sound comes back. Wi-Fi comes back if it was on.
- [ ] Charging blocks deep sleep and auto sleep.
- [x] Hold for a second: the screen goes dark at once, `<GAMECODE>.auto.dss` and a fresh `.sav` are written, and the Brick powers off. The next launch resumes from the auto state. (2026-11-05: the Brick restarted, and `AFFE.auto.dss` and the `.sav` were both written at 17:49, just before shutdown. Final Fantasy III relaunched.)
- [ ] Auto sleep (deferred by Howard, needs the cable out; Howard's screen timeout is 240 s): leave the game for the screen timeout with no input and it sleeps. Holding a D-pad direction for over a minute does not sleep.
- [ ] Battery drain overnight asleep: note the percentage before and after, and compare with minarch.
- [ ] Crash while asleep: tap to sleep, then from adb run `kill -9` on the `dsperate` process only (never on `launcher.elf` or `nextui.elf`). The backlight, volume and keymon, batmon and audiomon come back, and the launcher returns.

## Sources

- Community reports on DS and PSP on the Brick: retrocatalog.com/retro-handhelds/trimui-brick, handhelds.wiki/TrimUI_Brick, community.muos.dev/t/ppsspp-performance-optimisations/323, joeysretrohandhelds.com/reviews/trimui-brick-review/
- DraStic's licence status: gamingonlinux.com (2024-03, DraStic became free), retrohandhelds.gg (DraStic leaves the Play Store), emulation.gametechwiki.com
- Power and sleep, the NextUI DSperate pak: https://github.com/josegonzalez/minui-nintendo-dsperate-pak/blob/8c047207eacb05edbf9538928d54f08fd8c25051/overlay/minui.cpp, the same commit's `patches/0003-minui-integration.patch`, `TECHNICAL.md`, `launch.sh` and `README.md`
- Power and sleep, minui-power-control: https://github.com/ben16w/minui-power-control (`src/button-handler.go`, `bin/suspend`, `bin/shutdown`, issues #4, #11 and #20), used by https://github.com/josegonzalez/minui-nintendo-ds-pak, https://github.com/josegonzalez/minui-psp and https://github.com/ben16w/minui-psp
- Power and sleep, spruceOS at commit `00a9439f`: https://github.com/spruceUI/spruceOS (`spruce/scripts/power_button_watchdog.sh`, `spruce/scripts/save_poweroff.sh`, `spruce/scripts/platform/a133p.sh`, `spruce/scripts/platform/tina-stock/Brick.cfg`, `sleep_helper.sh`)
- DSperate v3.3.0: https://github.com/beebono/DSperate/tree/99618eb956c66083618395dbae9bb5cbf0241d16 (`src/frontend/sdl/main.cpp`, `lid.cpp`, `pacer.h`, `audio.cpp`)
