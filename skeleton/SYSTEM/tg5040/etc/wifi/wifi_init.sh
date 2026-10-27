#!/bin/sh

WIFI_INTERFACE="wlan0"
WPA_SUPPLICANT_CONF="/etc/wifi/wpa_supplicant.conf"

start() {
	# Unblock wifi via rfkill
	rfkill.elf unblock wifi 2>/dev/null
	
	# Create default wpa_supplicant.conf if it doesn't exist
	if [ ! -f "$WPA_SUPPLICANT_CONF" ]; then
		mkdir -p "$(dirname "$WPA_SUPPLICANT_CONF")"
		cat > "$WPA_SUPPLICANT_CONF" << 'EOF'
# cat /etc/wifi/wpa_supplicant.conf
ctrl_interface=/etc/wifi/sockets
disable_scan_offload=1
update_config=1
wowlan_triggers=any

EOF
	fi

	# Bramley: start wpa_supplicant here rather than through /etc/init.d/wpa_supplicant. That
	# script's readiness check never fails, so it always waited five half-second rounds (2.5 s)
	# before starting it. This waits only until wlan0 is up, then starts the same command.
	ifconfig $WIFI_INTERFACE up 2>/dev/null
	i=0
	while ! ifconfig $WIFI_INTERFACE 2>/dev/null | grep -q UP && [ $i -lt 20 ]; do
		usleep 100000
		ifconfig $WIFI_INTERFACE up 2>/dev/null
		i=$((i + 1))
	done
	if ! pidof wpa_supplicant > /dev/null 2>&1; then
		wpa_supplicant -B -i$WIFI_INTERFACE -Dnl80211 -c/etc/wifi/wpa_supplicant.conf \
			-I/etc/wifi/wpa_supplicant_overlay.conf -O/etc/wifi/sockets
		# as the init script asks of procd: keep it safe from the out-of-memory killer
		for pid in $(pidof wpa_supplicant); do echo -17 > /proc/$pid/oom_adj 2>/dev/null; done
	fi

	# Start DHCP client to obtain IP address. Bramley: in the background, because udhcpc otherwise
	# holds the script for its first attempts (about 6 s) while wpa_supplicant is still joining.
	if ! pidof udhcpc > /dev/null 2>&1; then	
		udhcpc -i $WIFI_INTERFACE -b > /dev/null 2>&1 &
	fi
}

stop() {
	/etc/init.d/wpa_supplicant stop

	rfkill.elf block wifi

	# Kill DHCP client
	killall udhcpc 2>/dev/null
}

case "$1" in
  start|"")
        start
        ;;
  stop)
        stop
        ;;
  *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac