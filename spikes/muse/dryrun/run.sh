#!/bin/sh
# Mac dry run of Muse Spike.pak inside an Ubuntu 24.04 arm64 container (FFmpeg 6.1).
# ALSA output goes to a WAV file instead of a speaker. Run from spikes/muse after `make`.
set -e
cd "$(dirname "$0")/.."
OUT="$(pwd)/build/dryrun"
rm -rf "$OUT" && mkdir -p "$OUT/spike"

docker run --rm --platform linux/arm64 \
	-v "$(pwd)/build/Muse Spike.pak":/pak:ro \
	-v "$(pwd)/dryrun":/dryrun:ro \
	-v "$OUT":/mnt/SDCARD \
	ubuntu:24.04 /bin/sh -c '
set -e
apt-get update -qq >/dev/null
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq ffmpeg libasound2-plugins >/dev/null
mkdir -p /mnt/SDCARD/spike
cp /dryrun/asound.conf /etc/asound.conf
ffmpeg -loglevel error -f lavfi -i "sine=frequency=440:duration=20" -c:a flac /mnt/SDCARD/spike/test.flac
ffmpeg -loglevel error -f lavfi -i "sine=frequency=660:duration=20" -c:a aac -f ipod /mnt/SDCARD/spike/test.m4b
ffmpeg -version | head -1 > /mnt/SDCARD/ffmpeg-version.txt
sh /pak/launch.sh
for f in /mnt/SDCARD/out-*.wav; do
	info=$(ffprobe -v error -select_streams a:0 -show_entries stream=sample_rate,channels -of csv=p=0 "$f")
	rate=${info%,*}; ch=${info#*,}; bytes=$(stat -c %s "$f")
	echo "$(basename "$f"): rate=$rate channels=$ch seconds=$(( (bytes - 44) / (rate * ch * 2) ))"
	ffmpeg -hide_banner -i "$f" -af astats -f null - 2>&1 | grep "RMS level dB" | tail -1 || true
done
'
echo "--- log: $OUT/spike/muse-spike.log"
