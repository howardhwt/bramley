#!/bin/sh
# Needs ffmpeg and Python with Pillow. Fills the desktop build's fake SD card (/var/tmp/nextui/sdcard) with demo content for
# screenshots: invented game titles, generated box art and album covers, a save-state
# screenshot and a few Paks. Nothing here is a real game or a trademark.
#
#   sh workspace/all/launcher/tools/demo-sd.sh          # move the current content aside, add demo
#   sh workspace/all/launcher/tools/demo-sd.sh restore  # put the previous content back
set -e
SD=/var/tmp/nextui/sdcard
KEEP=/var/tmp/nextui/sdcard-before-demo
FONT="$(cd "$(dirname "$0")/.." && pwd)/res/Inter-Bold.ttf"

if [ "$1" = "restore" ]; then
	[ -d "$KEEP" ] || { echo "nothing to restore"; exit 1; }
	for d in Roms Music Audiobooks Tools/desktop .userdata/shared/.minui .userdata/shared/.bramley; do
		rm -rf "$SD/$d"
		[ -e "$KEEP/$d" ] && mkdir -p "$(dirname "$SD/$d")" && mv "$KEEP/$d" "$SD/$d"
	done
	rm -rf "$KEEP"
	echo "restored"
	exit 0
fi

[ -d "$KEEP" ] && { echo "demo content is already in place; run with restore first"; exit 1; }
mkdir -p "$KEEP"
for d in Roms Music Audiobooks Tools/desktop .userdata/shared/.minui .userdata/shared/.bramley; do
	[ -e "$SD/$d" ] && mkdir -p "$(dirname "$KEEP/$d")" && mv "$SD/$d" "$KEEP/$d"
done

# Cover art: a two-colour gradient with the title in Inter, drawn by tools/cover.py (Pillow).
# Args: file w h colour1 colour2 title. PYTHON may point at an interpreter that has Pillow.
PYTHON="${PYTHON:-python3}"
cover() {
	"$PYTHON" "$(dirname "$0")/cover.py" "$1" "$2" "$3" "$4" "$5" "$6" "$FONT"
}

game() { # console-folder ext title c1 c2
	mkdir -p "$SD/Roms/$1/.media"
	touch "$SD/Roms/$1/$3.$2"
	cover "$SD/Roms/$1/.media/$3.png" 300 400 "$4" "$5" "$3"
}

game "Game Boy Advance (GBA)" gba "Starfall Tactics" 0x3b2667 0xe0629a
game "Game Boy Advance (GBA)" gba "Lantern Road" 0x14325c 0xf4a259
game "Game Boy Advance (GBA)" gba "Skyline Rally" 0x0f4c5c 0x5fbff9
game "Game Boy Advance (GBA)" gba "Moonlit Harbor" 0x1d2b53 0x7e9cd8
game "Super Nintendo (SFC)" sfc "Crystal Echo" 0x2b2d42 0x8d99ae
game "Super Nintendo (SFC)" sfc "Iron Valley" 0x3d1f1f 0xc65d3b
game "Super Nintendo (SFC)" sfc "Pixel Knights" 0x1b4332 0x95d5b2
game "Super Nintendo (SFC)" sfc "Seabreeze" 0x023e8a 0x90e0ef
game "Super Nintendo (SFC)" sfc "Ember Saga" 0x370617 0xf48c06
game "Super Nintendo (SFC)" sfc "Cloud Atlas Racing" 0x22223b 0xc9ada7
game "Super Nintendo (SFC)" sfc "Quiet Forest" 0x283618 0xdda15e
game "Game Boy (GB)" gb "Pocket Golf" 0x081c15 0x74c69d
game "Game Boy Color (GBC)" gbc "Paper Planes" 0x5a189a 0xff9e00
game "PlayStation (PS)" chd "Neon District" 0x10002b 0xe0aaff
game "Sega Genesis (MD)" md "Turbo Comet" 0x03045e 0x00b4d8
# A game without box art shows a title card.
touch "$SD/Roms/Super Nintendo (SFC)/Tiny Tower Builder.sfc"

# Continue Playing, newest first, with a save-state screenshot for the first game.
mkdir -p "$SD/.userdata/shared/.minui/GBA"
printf '%s\n' "/Roms/Game Boy Advance (GBA)/Starfall Tactics.gba" "/Roms/Super Nintendo (SFC)/Iron Valley.sfc" \
	"/Roms/Game Boy Color (GBC)/Paper Planes.gbc" "/Roms/PlayStation (PS)/Neon District.chd" \
	"/Roms/Super Nintendo (SFC)/Tiny Tower Builder.sfc" > "$SD/.userdata/shared/.minui/recent.txt"
echo 1 > "$SD/.userdata/shared/.minui/GBA/Starfall Tactics.gba.txt"
ffmpeg -v error -y -f lavfi -i "cellauto=s=240x160:rule=110:random_fill_ratio=0.5" -f lavfi \
	-i "gradients=s=240x160:c0=0x3b2667:c1=0xe0629a:d=1" -filter_complex "[1][0]blend=all_mode=overlay,format=bgr24" \
	-frames:v 1 "$SD/.userdata/shared/.minui/GBA/Starfall Tactics.gba.1.bmp"

# Music: albums with covers, and an audiobook.
album() { # artist album c1 c2 tracks...
	dir="$SD/Music/$1/$2"
	mkdir -p "$dir"
	cover "$dir/cover.jpg" 600 600 "$3" "$4" "$2"
	shift 4
	n=1
	for t in "$@"; do
		ffmpeg -v error -y -f lavfi -i "sine=frequency=$((220 + n * 55)):duration=$((150 + n * 20))" \
			-metadata title="$t" -c:a flac "$dir/0$n $t.flac"
		n=$((n + 1))
	done
}
album "Aurora Lane" "Night Drive" 0x0b132b 0x5bc0be "City Lights" "Midnight Run" "Neon Rain" "Last Exit"
album "Aurora Lane" "Morning Tide" 0x003049 0xfcbf49 "Low Sun" "Harbor" "Salt Air"
album "Paper Kites Club" "Sea Glass" 0x264653 0x2a9d8f "Shoreline" "Driftwood" "Tidepools" "Glass"
album "Velvet Static" "Afterglow" 0x3c096c 0xff6d00 "Ember" "Warm Static" "Slow Fade"
mkdir -p "$SD/Audiobooks/The Long Walk"
for c in 1 2 3; do
	ffmpeg -v error -y -f lavfi -i "sine=frequency=180:duration=600" -metadata title="Chapter $c" -c:a aac \
		"$SD/Audiobooks/The Long Walk/Chapter $c.m4a"
done

# Paks
mkdir -p "$SD/Tools/desktop"
for p in Settings Clock Battery Files "Game Tracker" Input "Pak Store" LedControl; do
	mkdir -p "$SD/Tools/desktop/$p.pak"
	printf '#!/bin/sh\n' > "$SD/Tools/desktop/$p.pak/launch.sh"
done
echo "demo content ready in $SD"
