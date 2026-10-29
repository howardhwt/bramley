#!/bin/sh
# Bramley: tidies a card that came from NextUI or from Bramley 0.x (the Pak), once
# (docs/plans/v1-standalone.md, step 6). Run by nextui-wrapper.sh before the launcher, on a Bramley
# card only. It removes only what it names: NextUI's files for other devices, NextUI's apps that
# Bramley replaces (and NextUI's Updater, which would put NextUI back over Bramley), and Bramley's
# old Pak. A folder goes only when nothing else is left in it, so anything the user added stays.
# Everything removed is listed in .userdata/shared/.bramley/migrate.log.
#
# SD_ROOT overrides the card's path, to try it on a copy (for example on the Mac).

SD="${SD_ROOT:-/mnt/SDCARD}"
STATE="$SD/.userdata/shared/.bramley"
MARK="$STATE/migrated-1"
LOG="$STATE/migrate.log"

[ -f "$MARK" ] && exit 0
head -n 1 "$SD/.system/version.txt" 2>/dev/null | grep -q '^Bramley' || exit 0 # the Pak on NextUI: not ours to tidy
mkdir -p "$STATE"
echo "$(date '+%F %T') tidying the card for Bramley" >> "$LOG"

# (shell functions share variables with their callers, hence the underscores)
remove() {
	for _r in "$@"; do
		if [ -e "$_r" ]; then
			rm -rf "$_r" && echo "removed ${_r#$SD/}" >> "$LOG"
		fi
	done
}

# a folder, only when empty
prune() {
	for _d in "$@"; do
		[ -d "$_d" ] && rmdir "$_d" 2>/dev/null && echo "removed empty ${_d#$SD/}" >> "$LOG"
	done
}

OTHERS="h700 tg5050 my355 desktop"

# NextUI's system for other devices (Bramley runs on the Brick only)
for p in $OTHERS; do
	remove "$SD/.system/$p" "$SD/.tmp_update/$p" "$SD/.tmp_update/$p.sh"
done
remove "$SD/h700/dmenu.bin"
prune "$SD/h700"

# NextUI's own apps: those Bramley does in Settings, the updater and the Pak Store
for p in tg5040 $OTHERS; do
	for pak in Battery Clock Input LedControl Bootlogo "Remove Loading" Updater "Pak Store"; do
		remove "$SD/Tools/$p/$pak.pak"
	done
done
# the other devices' copies of the apps Bramley keeps
for p in $OTHERS; do
	for pak in Settings Files "Game Tracker"; do
		remove "$SD/Tools/$p/$pak.pak"
	done
	prune "$SD/Tools/$p" "$SD/Emus/$p"
done

# Bramley 0.x, the Pak (and its name before the rename), and the Pak's assets from before the
# rename, which nothing reads now
remove "$SD/Tools/tg5040/Bramley.pak" "$SD/Tools/tg5040/Brick Hammer Launcher.pak" "$SD/.system/res/launcher"

touch "$MARK"
echo "$(date '+%F %T') done" >> "$LOG"
