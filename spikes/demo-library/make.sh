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
	[ -d "$S/Music.test" ] && rm -rf "$S/Music" && mv "$S/Music.test" "$S/Music"
	[ -f "$S/.userdata/shared/game_logs.sqlite.test" ] && mv "$S/.userdata/shared/game_logs.sqlite.test" "$S/.userdata/shared/game_logs.sqlite"
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
# music: Kimiko Ishizaka's Open Goldberg Variations (CC BY 4.0), two tracks and the cover
[ -d "$S/Music.test" ] || mv "$S/Music" "$S/Music.test"
rm -rf "$S/Music"
A="$S/Music/Kimiko Ishizaka/Open Goldberg Variations"
mkdir -p "$A"
BASE="https://archive.org/download/OpenGoldbergVariations/Kimiko%20Ishizaka%20-%20J.S.%20Bach-%20-Open-%20Goldberg%20Variations%2C%20BWV%20988%20%28Piano%29%20-%20"
curl -fsSL -o "$A/01 Aria.mp3" "${BASE}01%20Aria.mp3"
curl -fsSL -o "$A/02 Variatio 1 a 1 Clav.mp3" "${BASE}02%20Variatio%201%20a%201%20Clav..mp3"
curl -fsSL -o "$A/cover.jpg" "https://f4.bcbits.com/img/a1886980162_10.jpg"

# a week of play time for Activity, over the demo games
DB="$S/.userdata/shared/game_logs.sqlite"
[ -f "$DB.test" ] || { [ -f "$DB" ] && mv "$DB" "$DB.test"; }
rm -f "$DB"
"$PY" - "$DB" <<'PYEOF'
import sqlite3, sys, time
db = sqlite3.connect(sys.argv[1])
db.executescript("""CREATE TABLE rom(id INTEGER PRIMARY KEY, type TEXT, name TEXT, file_path TEXT, image_path TEXT,
  created_at INTEGER DEFAULT (strftime('%s', 'now')), updated_at INTEGER);
CREATE TABLE play_activity(rom_id INTEGER, play_time INTEGER, created_at INTEGER DEFAULT (strftime('%s', 'now')),
  updated_at INTEGER);""")
games = ["Game Boy (GB)/Tobu Tobu Girl Deluxe.gb", "Game Boy Color (GBC)/µCity.gbc", "Doom (PRBOOM)/Freedoom Phase 1.wad",
         "Sega Genesis (MD)/KleleAtoms MD.md", "Nintendo Entertainment System (FC)/Thwaite.nes",
         "Game Boy (GB)/Libbet and the Magic Floor.gb"]
for i, g in enumerate(games, 1):
    db.execute("INSERT INTO rom(id, name, file_path) VALUES (?, ?, ?)", (i, g.split("/")[1].rsplit(".", 1)[0], g))
# minutes per day, today last: sessions for each game
week = [[35, 0, 20, 0, 0, 10], [50, 25, 0, 0, 15, 0], [0, 40, 30, 0, 0, 0], [20, 0, 0, 25, 10, 0],
        [45, 30, 0, 0, 0, 15], [0, 0, 55, 20, 0, 0], [40, 35, 0, 0, 20, 0]]
now = int(time.time())
for d, mins in enumerate(week):
    day = now - (6 - d) * 86400 - 3600
    for g, m in enumerate(mins, 1):
        if m:
            db.execute("INSERT INTO play_activity(rom_id, play_time, created_at, updated_at) VALUES (?, ?, ?, ?)",
                       (g, m * 60, day - m * 60, day))
db.commit()
PYEOF

echo "demo library ready in $S"
