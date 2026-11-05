#!/bin/sh
# Spike pak for hand-testing DS emulators. The emulator is melonDS unless
# core.txt next to this script says desmume2015 (another libretro core under
# minarch) or dsperate (standalone DSperate in the dsperate/ folder).

CORES_PATH=$(dirname "$0")
EMU_EXE=melonds
[ -f "$CORES_PATH/core.txt" ] && EMU_EXE=$(tr -d ' \r\n' < "$CORES_PATH/core.txt")

###############################

EMU_TAG=$(basename "$(dirname "$0")" .pak)
ROM="$1"
mkdir -p "$BIOS_PATH/$EMU_TAG"
mkdir -p "$SAVES_PATH/$EMU_TAG"
mkdir -p "$CHEATS_PATH/$EMU_TAG"
HOME="$USERDATA_PATH"
cd "$HOME"

if [ "$EMU_EXE" = "dsperate" ]; then
	sh "$CORES_PATH/dsperate/run.sh" "$ROM" &> "$LOGS_PATH/$EMU_TAG-dsperate.txt"
	exit 0
fi

minarch.elf "$CORES_PATH/${EMU_EXE}_libretro.so" "$ROM" &> "$LOGS_PATH/$EMU_TAG-$EMU_EXE.txt"
