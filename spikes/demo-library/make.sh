#!/bin/sh
# Fills the Mac's fake card (/var/tmp/nextui/sdcard) with a demo library of free homebrew games,
# for screenshots and videos that show real games without anyone's trademarks or box art. The
# games and the licences of their art are in CREDITS.md. Only names and art go on the card: the
# game files are empty, since the desktop build does not play them. The fake card's own test
# files are moved aside first; "sh make.sh restore" puts them back.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
S=/var/tmp/nextui/sdcard
PY="${PYTHON:-python3}"

if [ "$1" = restore ]; then
	[ -d "$S/Roms.test" ] || { echo "nothing to restore"; exit 0; }
	rm -rf "$S/Roms"
	mv "$S/Roms.test" "$S/Roms"
	mv "$S/.userdata/shared/.minui/recent.txt.test" "$S/.userdata/shared/.minui/recent.txt"
	echo "restored"
	exit 0
fi

if [ ! -d "$S/Roms.test" ]; then
	mv "$S/Roms" "$S/Roms.test"
	cp "$S/.userdata/shared/.minui/recent.txt" "$S/.userdata/shared/.minui/recent.txt.test"
fi
rm -rf "$S/Roms"
# the desktop build lists only consoles with an emulator Pak; Doom has none there
P="$S/.system/desktop/paks/Emus"
[ -d "$P/PRBOOM.pak" ] || cp -R "$P/GB.pak" "$P/PRBOOM.pak"
# console folder | file name | art
while IFS='|' read -r console file art; do
	mkdir -p "$S/Roms/$console/.media"
	: > "$S/Roms/$console/$file"
	"$PY" - "$HERE/art/$art" "$S/Roms/$console/.media/${file%.*}.png" <<'PYEOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
im = im.resize((im.width * 4, im.height * 4), Image.NEAREST) if im.width < 400 else im
im.save(sys.argv[2])
PYEOF
done <<'LIST'
Doom (PRBOOM)|Freedoom Phase 1.wad|freedoom1.png
Doom (PRBOOM)|FreeDM.wad|freedm.png
Game Boy (GB)|Tobu Tobu Girl Deluxe.gb|tobu-dx.png
Game Boy (GB)|Libbet and the Magic Floor.gb|libbet.png
Game Boy Color (GBC)|µCity.gbc|ucity.png
Nintendo Entertainment System (FC)|Thwaite.nes|thwaite-game.png
Nintendo Entertainment System (FC)|Concentration Room.nes|croom.png
Nintendo Entertainment System (FC)|RHDE Furniture Fight.nes|rhde-game.png
Sega Genesis (MD)|KleleAtoms MD.md|klele.png
LIST
cat > "$S/.userdata/shared/.minui/recent.txt" <<'LIST'
/Roms/Game Boy (GB)/Tobu Tobu Girl Deluxe.gb
/Roms/Game Boy Color (GBC)/µCity.gbc
/Roms/Doom (PRBOOM)/Freedoom Phase 1.wad
/Roms/Sega Genesis (MD)/KleleAtoms MD.md
/Roms/Nintendo Entertainment System (FC)/Thwaite.nes
/Roms/Game Boy (GB)/Libbet and the Magic Floor.gb
LIST
echo "demo library ready in $S"
