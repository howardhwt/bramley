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
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
VERSION="$(cat "$HERE/VERSION")"
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
cp "$PAK/launcher.elf" "$PAK/muse" "$PAK/minarch.elf" "$BIN/"
cp "$PAK/libmsettings.so" "$SYS/.system/tg5040/lib/"
cp "$PAK/wifi_init.sh" "$SYS/.system/tg5040/etc/wifi/"
chmod +x "$BIN/nextui.elf" "$BIN/launcher.elf" "$BIN/muse" "$BIN/minarch.elf"
echo "$VERSION" > "$BIN/.bramley-version"

echo "== Bramley's apps: Settings replaced; NextUI's tools that Bramley does in Settings, removed"
cp "$PAK/settings.elf" "$SYS/Tools/tg5040/Settings.pak/"
for t in Battery Clock Input LedControl Bootlogo "Remove Loading"; do
	rm -rf "$SYS/Tools/tg5040/$t.pak" "$CARD/Tools/tg5040/$t.pak"
done
# Files.pak and Game Tracker.pak stay: their cards open Bramley's Files and Activity.

echo "== Bramley's look"
mkdir -p "$SYS/.system/res/bramley"
cp -R "$PAK/res/"* "$SYS/.system/res/bramley/"
cp "$PAK/fonts/"*.ttf "$SYS/.system/res/"

echo "== the install screen: Bramley's mark in place of NextUI's logo"
cp "$ROOT/docs/brand/bramley-install-logo.png" "$SYS/.tmp_update/tg5040/logo.png"

echo "== names users see"
sed -i '' 's/TEXT:Updating NextUI/TEXT:Updating Bramley/; s/TEXT:Installing NextUI/TEXT:Setting up Bramley/' "$SYS/.tmp_update/tg5040.sh"
sed -i '' 's/(NextUI)/(Bramley)/' "$SYS/.system/tg5040/etc/bluetooth/bt_init.sh"
printf 'Bramley %s\n%s\n' "$VERSION" "$(git -C "$ROOT" rev-parse --short HEAD)" > "$SYS/.system/version.txt"

echo "== card root"
rm -f "$CARD/nextui.pak_store.pakz" "$CARD/nextui.updater.pakz"   # NextUI's store and updater: not shipped
cp "$PAK/README.txt" "$CARD/README.txt"
cp "$ROOT/THIRD-PARTY-LICENSES.md" "$ROOT/LICENSE" "$CARD/" 2>/dev/null || true

echo "== zipping"
(cd "$SYS" && rm -f "$OUT/MinUI.zip" && zip -q -r -X "$OUT/MinUI.zip" .system .tmp_update Tools)
cp "$OUT/MinUI.zip" "$CARD/MinUI.zip"
mkdir -p "$ROOT/releases"
rm -f "$ROOT/releases/Bramley-$VERSION-card.zip" "$ROOT/releases/Bramley-$VERSION-update.zip"
(cd "$CARD" && zip -q -r -X "$ROOT/releases/Bramley-$VERSION-card.zip" .)
cp "$OUT/MinUI.zip" "$ROOT/releases/Bramley-$VERSION-update.zip"
ls -l "$ROOT/releases/Bramley-$VERSION-card.zip" "$ROOT/releases/Bramley-$VERSION-update.zip"
