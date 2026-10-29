# Bramley v1.0: a standalone card

Status: plan, 2026-10-29. Decisions below were made with Howard; nothing is built yet.

## Goal

Bramley installs on a TrimUI Brick or Brick Hammer running TrimUI's stock firmware, with no NextUI installed first. The user copies Bramley to a card, turns the device on, sees Bramley's install screen, and lands on Bramley's home screen. Updates come from Bramley's own updater.

## What we learned

Our repo still contains NextUI's full card build. The top-level `makefile` already builds every program, the emulator cores (from source, with patches), the install scripts and a release zip, for tg5040 and two other platforms. Bramley today only builds a Pak that is copied over a NextUI card.

So v1.0 is mostly packaging, not new technology: take NextUI's card build, put Bramley's programs in it, and replace NextUI's name, logos and updater.

How a card starts today:

1. On first boot, TrimUI's firmware runs `trimui/app/MainUI` from the card. It is a script that replaces the device's `/usr/trimui/bin/runtrimui.sh` with one that runs the card's `.tmp_update/updater` on every boot (TrimUI's original is kept as `runtrimui-original.sh`).
2. `updater` runs `.tmp_update/tg5040.sh`, which shows the "Installing NextUI" screen, unpacks `MinUI.zip` (the system) and starts the launcher through `MinUI.pak/launch.sh`.
3. `launch.sh` starts the background helpers (keymon, batmon, audiomon), Wi-Fi and Bluetooth, then loops the launcher and whatever it asks to run next.

## Decisions (2026-10-29)

- **Devices:** the TrimUI Brick and Brick Hammer only (tg5040, 1024x768). Other devices NextUI supports are out of scope.
- **Cores:** ship what NextUI ships, after checking each core's license; GPL cores ship with their source and our patches.
- **Pak Store:** dropped from v1.0. Paks copied to `Tools/` by hand still show in Apps.
- **Release format:** one zip to extract onto a FAT32 card, for new installs and updates alike. No card image.
- **First-boot setup:** Wi-Fi (titled "Welcome to Bramley"), then time zone, then home with a hint for SELECT and MENU. Every step skips with B. No separate welcome screen.
- **Updates:** a Software Update page in Settings, plus a quiet check at most once a day while the launcher is on the home screen and Wi-Fi is already on (never turning Wi-Fi on for it). A dot on Settings shows an update; downloading and installing only happen when the user chooses.
- **Motion preview:** the setup flow is shown as a video rendered from the real drawing code before it is built.

## What v1.0 needs

1. **A Bramley card build.** One make target that builds the whole card for tg5040 with Bramley's launcher, Settings, minarch and libmsettings in place of NextUI's, and Bramley's assets. The Pak build stays for 0.x users until v1.0.
2. **Bramley's install and update screen.** Replace `show2` and its "Installing NextUI" text with Bramley's: the mark, the bar and plain words ("Setting up Bramley…", "Updating Bramley…"). It should match the start-up screens already shipped.
3. **First-boot setup.** On the first start: language (later), Wi-Fi, time zone and date, and a short welcome. Optional: skip with B.
4. **Bramley's updater.** Checks GitHub releases for howardhwt/bramley, downloads the update on Wi-Fi with progress, verifies its SHA-256, and applies it on the next start through the install screen. Replaces NextUI's updater, which comes as a prebuilt download we do not control.
5. **Rename what users see.** The install text, the Bluetooth name ("Trimui Brick (NextUI)"), README, version file, release names, logos. Internal names such as `MinUI.pak` can stay for now; they are not shown.
6. **Content to ship or drop.**
   - Cores: build from source as NextUI does. Ship only cores whose licenses allow free redistribution; check each one.
   - BIOS: none, as now. Users bring their own.
   - Tools: Settings (ours), Files (ours), Activity (ours). Drop NextCommander, Clock, Battery, Input, LedControl, Game Tracker, Remove Loading.
   - Bootlogo app and its Nintendo, Sony and Atari logos: drop.
   - Pak Store: keep or replace (open question).
7. **Moving from 0.x.** A user on NextUI plus the Bramley Pak can install the v1.0 card over it and keep saves, states, ROMs, music and settings.

## Changes to the device itself

Like NextUI, Bramley v1.0 changes files outside the SD card: `runtrimui.sh` (to start from the card), the boot logo and `/etc/splash.png` (already done in 0.2.2), and the Wi-Fi and Bluetooth start scripts. Each needs a backup and a way back to stock. These need Howard's approval (CLAUDE.md).

## License

Bramley stays PolyForm Noncommercial 1.0.0, free, and credits NextUI. The cores have their own licenses; some forbid commercial use, which is fine for a free release, and GPL cores need their source and our patches published with the release. Each core is checked before it ships.

## Open questions

- How first-boot Wi-Fi setup works with the keyboard and the radio's slow start.
- Trademark check in the EU and UK registers before the v1.0 name goes public.

## Order of work (proposed)

1. Bramley card build target, producing a card that boots straight into Bramley (with NextUI's install screen still).
2. Bramley's install screen and renaming.
3. Content trim and core license check. Done 2026-10-29: licenses in `docs/plans/cores-licenses.md`; every core pinned and built by Bramley, matching NextUI 6.15.0's. Still open: the license texts on the card, and the legal question in that file. Note that the update zip (MinUI.zip) only carries the six system cores; the cores in `Emus/` are outside it, so the updater must handle them.
4. Updater.
5. First-boot setup.
6. Upgrade path from 0.x, then a public beta.
