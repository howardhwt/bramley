#!/bin/sh
# Runs standalone DSperate for the spike's NDS.pak, when NDS.pak/core.txt says
# dsperate. Called by NDS.pak/launch.sh with the ROM path, and with the
# NextUI environment MinUI.pak/launch.sh exports (SDCARD_PATH, BIOS_PATH,
# SAVES_PATH, USERDATA_PATH, LD_LIBRARY_PATH with /usr/trimui/lib for SDL2).

DIR=$(cd "$(dirname "$0")" && pwd)
ROM="$1"

# Config, and the per-game layout files DSperate writes beside it, under
# .userdata/tg5040/NDS-dsperate/dsperate/.
DSP_HOME="$USERDATA_PATH/NDS-dsperate"
CONFIG="$DSP_HOME/dsperate/dsperate.ini"
DSP_SAVES="$SAVES_PATH/NDS/dsperate"
DSP_STATES="$SHARED_USERDATA_PATH/NDS-dsperate"
mkdir -p "$DSP_HOME/dsperate" "$DSP_SAVES" "$DSP_STATES" "$BIOS_PATH/NDS" "$SDCARD_PATH/Screenshots" "$CHEATS_PATH/NDS"

if [ ! -f "$CONFIG" ]; then
	echo "seeding $CONFIG"
	sed \
		-e "s|__BIOS__|$BIOS_PATH/NDS|g" \
		-e "s|__ROMS__|$ROMS_PATH/Nintendo DS (NDS)|g" \
		-e "s|__SAVES__|$DSP_SAVES|g" \
		-e "s|__STATES__|$DSP_STATES|g" \
		-e "s|__SCREENSHOTS__|$SDCARD_PATH/Screenshots|g" \
		-e "s|__CHEATS__|$CHEATS_PATH/NDS/usrcheat.dat|g" \
		"$DIR/dsperate.ini" > "$CONFIG"
	sync
fi

export HOME="$DSP_HOME"
export XDG_CONFIG_HOME="$DSP_HOME"

# The Brick's pad is "TRIMUI Player1", which reports itself as an Xbox 360 pad
# (bus 3, 045e:028e, version 0114). This mapping, for that GUID, replaces SDL's
# built-in Xbox one. It is positional (SDL's a is the south button), from
# josegonzalez/minui-nintendo-dsperate-pak's Brick profile: TrimUI buttons from
# 0, MENU as guide, SELECT as back, L2 and R2 as axes 2 and 5, F1 and F2 as
# the stick clicks.
export SDL_GAMECONTROLLERCONFIG="030000005e0400008e02000014010000,TRIMUI Brick,a:b1,b:b0,x:b3,y:b2,back:b6,guide:b8,start:b7,leftshoulder:b4,rightshoulder:b5,lefttrigger:a2,righttrigger:a5,leftstick:b9,rightstick:b10,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,platform:Linux,"

# DS_FPS logs the frame rate, with emulation and draw time, to this log once a
# second, for the measurement.
export DS_FPS=1

echo "rom: $ROM"
cd "$DIR" || exit 1
./dsperate "$ROM" --fullscreen
STATUS=$?
echo "dsperate exited with $STATUS"

# Bramley: DSperate handles the power key itself (patches/dsperate-bramley-power.patch).
# While the Brick sleeps it keeps this marker, with the backlight off, the
# volume at zero and keymon, batmon and audiomon stopped. If it exited or
# crashed then, turn everything back on here.
if [ -f /tmp/bramley_dsperate_asleep ]; then
	./dsperate --bramley-wake
	for helper in keymon.elf batmon.elf audiomon.elf; do
		killall -CONT "$helper" 2>/dev/null
	done
	rm -f /tmp/bramley_dsperate_asleep
fi
sync
exit $STATUS
