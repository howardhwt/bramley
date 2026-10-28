#!/bin/sh
# Builds the release zip: Tools/tg5040/Bramley.pak, ready to extract to an SD card that runs
# NextUI. Needs Docker or OrbStack (the tg5040 toolchain), curl and zip.
#
#   sh workspace/all/launcher/package.sh
#
# Output: releases/Bramley-<version>.zip, plus its SHA-256 in a .sha256 file.
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
VERSION="$(cat "$HERE/VERSION")"
NAME="Bramley-$VERSION"
OUT="$ROOT/build/launcher-release"
PAK="$OUT/Tools/tg5040/Bramley.pak"

# The Muse playback daemon, prebuilt by TortOS (MIT). Same release and checksum as spikes/muse.
TORTOS_TAG=v1.6.0
TORTOS_ZIP=TortOS-$TORTOS_TAG.zip
TORTOS_SHA256=54bd2cb7552fd3c7e27609f6dd6549d356cf06c5e92be18921fbc360c51c076a
TORTOS_URL=https://github.com/ericreinsmidt/TortOS/releases/download/$TORTOS_TAG/$TORTOS_ZIP

RUNTIME="$(command -v docker || command -v podman)"
[ -n "$RUNTIME" ] || { echo "Docker or OrbStack is needed for the tg5040 toolchain" >&2; exit 1; }

echo "== building the launcher, Settings and minarch $VERSION for tg5040"
"$RUNTIME" run --rm -v "$ROOT/workspace":/root/workspace ghcr.io/loveretro/tg5040-toolchain:latest /bin/bash -c \
	'. ~/.bashrc && export PLATFORM=tg5040 UNION_PLATFORM=tg5040 && cd /root/workspace/all && \
	 (cd ../tg5040/libmsettings && make) && (cd launcher && make clean && make) && (cd settings && make clean && make) && (cd minarch && make)'

echo "== fetching muse from TortOS $TORTOS_TAG"
mkdir -p "$OUT/cache"
if [ ! -f "$OUT/cache/$TORTOS_ZIP" ]; then
	curl -fL -o "$OUT/cache/$TORTOS_ZIP.part" "$TORTOS_URL"
	mv "$OUT/cache/$TORTOS_ZIP.part" "$OUT/cache/$TORTOS_ZIP"
fi
echo "$TORTOS_SHA256  $OUT/cache/$TORTOS_ZIP" | shasum -a 256 -c -
unzip -o -q -j "$OUT/cache/$TORTOS_ZIP" TortOS/muse -d "$OUT/cache"

echo "== assembling the Pak"
rm -rf "$OUT/Tools"
mkdir -p "$PAK/fonts"
cp "$HERE/package/launch.sh" "$PAK/"
cp "$HERE/build/tg5040/launcher.elf" "$OUT/cache/muse" "$HERE/nextui-wrapper.sh" "$PAK/"
cp "$HERE/../settings/build/tg5040/settings.elf" "$HERE/../minarch/build/tg5040/minarch.elf" "$PAK/"
cp "$ROOT/skeleton/SYSTEM/tg5040/etc/wifi/wifi_init.sh" "$ROOT/workspace/tg5040/libmsettings/libmsettings.so" "$PAK/"
mkdir -p "$PAK/res"
cp -R "$HERE/res/settings" "$HERE/res/wallpapers" "$PAK/res/"
cp "$HERE/res/Inter-Regular.ttf" "$HERE/res/Inter-SemiBold.ttf" "$HERE/res/Inter-Bold.ttf" "$PAK/fonts/"
cp "$HERE/VERSION" "$PAK/"
cp "$ROOT/THIRD-PARTY-LICENSES.md" "$HERE/res/Inter-LICENSE.txt" "$PAK/"
cp "$ROOT/LICENSE" "$PAK/LICENSE-NextUI.txt"
cp "$HERE/package/README.txt" "$PAK/"
chmod +x "$PAK/launch.sh" "$PAK/launcher.elf" "$PAK/settings.elf" "$PAK/minarch.elf" "$PAK/muse" "$PAK/nextui-wrapper.sh"
file "$PAK/launcher.elf" "$PAK/settings.elf" "$PAK/minarch.elf" "$PAK/muse" | sed 's/,.*//'

echo "== zipping"
mkdir -p "$ROOT/releases"
rm -f "$ROOT/releases/$NAME.zip"
(cd "$OUT" && zip -q -r -X "$ROOT/releases/$NAME.zip" Tools)
(cd "$ROOT/releases" && shasum -a 256 "$NAME.zip" > "$NAME.zip.sha256")
ls -l "$ROOT/releases/$NAME.zip"
cat "$ROOT/releases/$NAME.zip.sha256"
