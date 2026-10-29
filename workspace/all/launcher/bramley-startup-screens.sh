#!/bin/sh
# Bramley: sets the device's start-up screens to Bramley's, or puts TrimUI's back. These are the
# boot logo on the boot partition and the firmware's loading picture (/etc/splash.png). Both are
# outside the SD card, so the stock ones are kept on the card first (.userdata/shared/.bramley),
# each write is read back and checked, and a failure leaves the stock picture in place. The same
# steps as the 0.x Pak's installer (package/launch.sh), approved by Howard for the card on
# 2026-10-29.
#   bramley-startup-screens.sh install | restore
# On a Bramley card, nextui-wrapper.sh runs "install" once.

SD=/mnt/SDCARD
STATE="$SD/.userdata/shared/.bramley"
ART="$SD/.system/res/bramley/brand"
LOG="${LOGS_PATH:-$SD/.userdata/tg5040/logs}/bramley.txt"
BOOTDEV=/dev/mmcblk0p1
BOOTMNT=/tmp/bramley-boot
mkdir -p "$STATE" "$(dirname "$LOG")"

say() { echo "$(date '+%F %T') $*" >> "$LOG"; }
same() { [ "$(md5sum < "$1")" = "$(md5sum < "$2")" ]; }

boot_logo() { # install | restore
	mkdir -p "$BOOTMNT"
	mount -t vfat "$BOOTDEV" "$BOOTMNT" 2>/dev/null || { say "boot logo: could not open the boot partition; left as it was"; return; }
	if [ "$1" = install ] && same "$ART/bootlogo.bmp" "$BOOTMNT/bootlogo.bmp"; then
		# already Bramley's (from another Bramley card): never keep it as TrimUI's
		say "boot logo: already Bramley's"
	elif [ "$1" = install ]; then
		[ -f "$STATE/bootlogo-stock.bmp" ] || cp -f "$BOOTMNT/bootlogo.bmp" "$STATE/bootlogo-stock.bmp"
		if [ -f "$STATE/bootlogo-stock.bmp" ] && cp -f "$ART/bootlogo.bmp" "$BOOTMNT/bootlogo.bmp.new" && sync &&
			same "$ART/bootlogo.bmp" "$BOOTMNT/bootlogo.bmp.new" && mv -f "$BOOTMNT/bootlogo.bmp.new" "$BOOTMNT/bootlogo.bmp"; then
			say "boot logo: Bramley's"
		else
			rm -f "$BOOTMNT/bootlogo.bmp.new"
			say "boot logo: could not write; the stock one stays"
		fi
	elif [ -f "$STATE/bootlogo-stock.bmp" ]; then
		cp -f "$STATE/bootlogo-stock.bmp" "$BOOTMNT/bootlogo.bmp" && sync && same "$STATE/bootlogo-stock.bmp" "$BOOTMNT/bootlogo.bmp" &&
			rm -f "$STATE/bootlogo-stock.bmp" && say "boot logo: stock one back"
	fi
	sync
	umount "$BOOTMNT"
}

splash() { # install | restore
	if [ "$1" = install ] && [ -f /etc/splash.png ] && same "$ART/splash.png" /etc/splash.png; then
		say "splash: already Bramley's"
	elif [ "$1" = install ]; then
		[ -f /etc/splash.png ] || return
		[ -f "$STATE/splash-stock.png" ] || cp -f /etc/splash.png "$STATE/splash-stock.png"
		if [ -f "$STATE/splash-stock.png" ] && cp -f "$ART/splash.png" /etc/splash.png.new && sync &&
			same "$ART/splash.png" /etc/splash.png.new && chmod 775 /etc/splash.png.new && mv -f /etc/splash.png.new /etc/splash.png; then
			say "splash: Bramley's"
		else
			rm -f /etc/splash.png.new
			say "splash: could not write; the stock one stays"
		fi
	elif [ -f "$STATE/splash-stock.png" ]; then
		cp -f "$STATE/splash-stock.png" /etc/splash.png && sync && same "$STATE/splash-stock.png" /etc/splash.png &&
			rm -f "$STATE/splash-stock.png" && say "splash: stock one back"
	fi
	sync
}

case "$1" in
install | restore)
	[ -f "$ART/bootlogo.bmp" ] && [ -f "$ART/splash.png" ] || { say "start-up screens: Bramley's pictures are missing"; exit 1; }
	boot_logo "$1"
	splash "$1"
	;;
*)
	echo "usage: $0 install | restore" >&2
	exit 2
	;;
esac
