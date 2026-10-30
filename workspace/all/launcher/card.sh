#!/bin/sh
# Builds a whole Bramley card for the TrimUI Brick and Brick Hammer: a zip that, extracted onto a
# card, starts Bramley without NextUI installed first (docs/plans/v1-standalone.md, step 1).
#
#   sh workspace/all/launcher/card.sh
#
# For now the card starts from NextUI 6.15.0's release (the version Bramley's code is based on),
# pinned by its SHA-256, for the emulator cores and the programs Bramley has not replaced; building
# those from source comes later. Bramley's programs, assets and names go over it, and what Bramley
# does not ship is removed. Needs Docker or OrbStack, gh or curl, and zip.
#
# Output in releases/:
#   Bramley-<version>-card.zip   the whole card, to extract onto an empty FAT32 card
#   Bramley-<version>-update.zip MinUI.zip only, to put at the root of a card that already runs
#                                 NextUI or Bramley; the device installs it on the next start
#   bramley-update.txt           what Software Update reads from the latest release: the version,
#                                 the card zip's name, size and SHA-256, and release-notes.txt
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
VERSION="${BRAMLEY_VERSION:-$(cat "$HERE/VERSION")}"   # BRAMLEY_VERSION overrides, for test builds
OUT="$ROOT/build/card"
CACHE="$ROOT/build/card-cache"

BASE_TAG=v6.15.0
BASE_ZIP=NextUI-20261013-0-all.zip
BASE_SHA256=d5edceaa92d0264f34da87f017a40fb8244dc3081f894f17c6979ffe94c4c38a
BASE_URL=https://github.com/LoveRetro/NextUI/releases/download/$BASE_TAG/$BASE_ZIP

# Bramley's programs: built (and muse fetched) by the Pak packager, which this reuses.
echo "== building Bramley's programs"
sh "$HERE/package.sh" >/dev/null
PAK="$ROOT/build/launcher-release/Tools/tg5040/Bramley.pak"
[ -f "$PAK/launcher.elf" ] || { echo "the Pak build did not produce launcher.elf" >&2; exit 1; }

echo "== fetching NextUI $BASE_TAG as the base"
mkdir -p "$CACHE"
if [ ! -f "$CACHE/$BASE_ZIP" ]; then
	curl -fL -o "$CACHE/$BASE_ZIP.part" "$BASE_URL"
	mv "$CACHE/$BASE_ZIP.part" "$CACHE/$BASE_ZIP"
fi
echo "$BASE_SHA256  $CACHE/$BASE_ZIP" | shasum -a 256 -c -

echo "== unpacking"
rm -rf "$OUT"
mkdir -p "$OUT/card" "$OUT/system"
unzip -q "$CACHE/$BASE_ZIP" -d "$OUT/card"
unzip -q "$OUT/card/MinUI.zip" -d "$OUT/system"
rm -f "$OUT/card/MinUI.zip"
CARD="$OUT/card"
SYS="$OUT/system"
BIN="$SYS/.system/tg5040/bin"

echo "== Brick only: removing other devices"
rm -rf "$SYS/.system/h700" "$SYS/.system/tg5050" "$SYS/.system/desktop"
rm -rf "$SYS/.tmp_update/h700" "$SYS/.tmp_update/h700.sh" "$SYS/.tmp_update/tg5050" "$SYS/.tmp_update/tg5050.sh"
rm -rf "$CARD/h700" "$CARD/Emus/h700" "$CARD/Emus/tg5050" "$CARD/Tools/h700" "$CARD/Tools/tg5050" "$CARD/Tools/desktop"
rm -rf "$SYS/Tools/h700" "$SYS/Tools/tg5050" "$SYS/Tools/desktop"

echo "== Bramley's programs"
mv "$BIN/nextui.elf" "$BIN/nextui-stock.elf"         # the safety net's fallback
cp "$PAK/nextui-wrapper.sh" "$BIN/nextui.elf"         # starts launcher.elf
cp "$HERE/bramley-migrate.sh" "$BIN/"                # tidies a card upgraded from NextUI or 0.x
cp "$HERE/bramley-startup-screens.sh" "$BIN/"        # Bramley's boot logo and loading picture
cp "$PAK/launcher.elf" "$PAK/muse" "$PAK/minarch.elf" "$BIN/"
cp "$PAK/libmsettings.so" "$SYS/.system/tg5040/lib/"
cp "$PAK/wifi_init.sh" "$SYS/.system/tg5040/etc/wifi/"
chmod +x "$BIN/nextui.elf" "$BIN/bramley-migrate.sh" "$BIN/bramley-startup-screens.sh" "$BIN/launcher.elf" "$BIN/muse" "$BIN/minarch.elf"
echo "$VERSION" > "$BIN/.bramley-version"

echo "== Bramley's apps: Settings replaced; NextUI's tools that Bramley does in Settings, removed"
cp "$PAK/settings.elf" "$SYS/Tools/tg5040/Settings.pak/"
for t in Battery Clock Input LedControl Bootlogo "Remove Loading"; do
	rm -rf "$SYS/Tools/tg5040/$t.pak" "$CARD/Tools/tg5040/$t.pak"
done
# Files.pak and Game Tracker.pak stay: their cards open Bramley's Files and Activity.

echo "== the emulator cores: Bramley's own builds, pinned (docs/plans/cores-licenses.md)"
# Built with: docker run ... tg5040-toolchain ... 'cd /root/workspace/tg5040/cores && make'
# (the pins in workspace/tg5040/cores/makefile rebuild NextUI 6.15.0's cores exactly).
COREOUT="$ROOT/workspace/tg5040/cores/output"
find "$SYS/.system/tg5040/cores" "$CARD/Emus/tg5040" -name '*_libretro.so' | while read -r so; do
	ours="$COREOUT/$(basename "$so")"
	[ -f "$ours" ] || { echo "no Bramley build of $(basename "$so"); build the cores first" >&2; exit 1; }
	cp "$ours" "$so"
done
sh "$HERE/cores-source.sh" > "$CARD/CORES-SOURCE.md"
# each core's license files, from its source at the pinned commit
CORESRC="$ROOT/workspace/tg5040/cores/src"
for dir in "$CORESRC"/*/; do
	core=$(basename "$dir")
	(cd "$dir" && find . -maxdepth 3 \( -iname 'license*' -o -iname 'copying*' \) -type f | grep -v -i '/test\|/deps/\|/3rdparty\|/platforms/ios') |
	while read -r f; do
		mkdir -p "$CARD/Licenses/$core/$(dirname "$f")"
		cp "$dir/$f" "$CARD/Licenses/$core/$f"
	done
	[ -d "$CARD/Licenses/$core" ] || { echo "no license file found for the $core core" >&2; exit 1; }
done
[ -d "$CORESRC/mgba/res/licenses" ] && cp -R "$CORESRC/mgba/res/licenses" "$CARD/Licenses/mgba/third-party"

echo "== Bramley's look"
mkdir -p "$SYS/.system/res/bramley"
cp -R "$PAK/res/"* "$SYS/.system/res/bramley/"
cp "$PAK/fonts/"*.ttf "$SYS/.system/res/"

cp "$ROOT/docs/brand/bramley-bootlogo.bmp" "$SYS/.system/res/bramley/brand/bootlogo.bmp"
cp "$ROOT/docs/brand/bramley-splash.png" "$SYS/.system/res/bramley/brand/splash.png"

echo "== the install screen: Bramley's mark in place of NextUI's logo"
cp "$ROOT/docs/brand/bramley-install-logo.png" "$SYS/.tmp_update/tg5040/logo.png"

echo "== names users see"
sed -i '' 's/TEXT:Updating NextUI/TEXT:Updating Bramley/; s/TEXT:Installing NextUI/TEXT:Setting up Bramley/' "$SYS/.tmp_update/tg5040.sh"
# a fresh install (no .system yet) asks the launcher for first-boot setup (lsetup.h); before the next line, which gives the add-on packages the same words
sed -i '' '/TEXT:Setting up Bramley" > \/tmp\/show2.fifo/a\
		mkdir -p "$SDCARD_PATH/.userdata/shared/.bramley" \&\& touch "$SDCARD_PATH/.userdata/shared/.bramley/setup-pending"
' "$SYS/.tmp_update/tg5040.sh"
# the add-on packages (the Bluetooth update) showed their full path; plain words instead
sed -i '' 's/TEXT:Extracting \$pakz/TEXT:Setting up Bramley/; s/TEXT:Installing \$pakz/TEXT:Setting up Bramley/' "$SYS/.tmp_update/tg5040.sh"
sed -i '' 's/(NextUI)/(Bramley)/' "$SYS/.system/tg5040/etc/bluetooth/bt_init.sh"
printf 'Bramley %s\n%s\n' "$VERSION" "$(git -C "$ROOT" rev-parse --short HEAD)" > "$SYS/.system/version.txt"
# A new card runs the first-boot copy in trimui/app, which upstream matches the one in MinUI.zip.
TU="$CARD/trimui/app/.tmp_update"
rm -rf "$TU"
cp -R "$SYS/.tmp_update" "$TU"

echo "== card root"
rm -f "$CARD/nextui.pak_store.pakz" "$CARD/nextui.updater.pakz"   # NextUI's store and updater: not shipped
cp "$HERE/card/README.txt" "$CARD/README.txt"
cp "$ROOT/THIRD-PARTY-LICENSES.md" "$ROOT/LICENSE" "$HERE/res/Inter-LICENSE.txt" "$CARD/"

# a fingerprint of the card outside MinUI.zip: when a card's matches a release's, Software Update
# downloads only the system zip (bramley_update.c)
(cd "$CARD" && find Emus Licenses CORES-SOURCE.md README.txt LICENSE THIRD-PARTY-LICENSES.md Inter-LICENSE.txt -type f -print0 |
	LC_ALL=C sort -z | xargs -0 shasum -a 256) | shasum -a 256 | cut -c1-16 > "$CARD/.bramley-extras-id"

echo "== zipping"
(cd "$SYS" && rm -f "$OUT/MinUI.zip" && zip -q -r -X "$OUT/MinUI.zip" .system .tmp_update Tools)
cp "$OUT/MinUI.zip" "$CARD/MinUI.zip"
mkdir -p "$ROOT/releases"
rm -f "$ROOT/releases/Bramley-$VERSION-card.zip" "$ROOT/releases/Bramley-$VERSION-update.zip"
(cd "$CARD" && zip -q -r -X "$ROOT/releases/Bramley-$VERSION-card.zip" .)
cp "$OUT/MinUI.zip" "$ROOT/releases/Bramley-$VERSION-update.zip"
ZIPF="Bramley-$VERSION-card.zip"
{
	echo "bramley-update 1"
	echo "version $VERSION"
	echo "file $ZIPF"
	echo "size $(stat -f%z "$ROOT/releases/$ZIPF")"
	echo "sha256 $(shasum -a 256 "$ROOT/releases/$ZIPF" | cut -d' ' -f1)"
	echo "system Bramley-$VERSION-update.zip"
	echo "system_size $(stat -f%z "$ROOT/releases/Bramley-$VERSION-update.zip")"
	echo "system_sha256 $(shasum -a 256 "$ROOT/releases/Bramley-$VERSION-update.zip" | cut -d' ' -f1)"
	echo "extras $(cat "$CARD/.bramley-extras-id")"
	echo "notes"
	cat "$HERE/release-notes.txt"
} > "$ROOT/releases/bramley-update.txt"
ls -l "$ROOT/releases/$ZIPF" "$ROOT/releases/Bramley-$VERSION-update.zip" "$ROOT/releases/bramley-update.txt"
