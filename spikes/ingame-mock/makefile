# Desktop-only mockup of a Bramley in-game menu, built from the launcher's drawing code.
#   gmake PLATFORM=desktop CROSS_COMPILE=$HOME/.local/nextui-gcc/ PREFIX=/opt/homebrew PREFIX_LOCAL=/var/tmp/nextui UNION_PLATFORM=desktop UNAME_S=Darwin
W = ../../workspace
include $(W)/$(PLATFORM)/platform/makefile.env
L = $(W)/all/launcher
C = $(W)/all/common
SOURCE = mock.c $(L)/lgfx.c $(C)/scaler.c $(C)/utils.c $(C)/config.c $(C)/api.c $(W)/$(PLATFORM)/platform/platform.c
CFLAGS += $(OPT) -I$(L) -I$(C) -I$(W)/$(PLATFORM)/platform -DPLATFORM=\"$(PLATFORM)\" -std=gnu99
LDFLAGS += -lmsettings
all:
	mkdir -p build
	$(CROSS_COMPILE)gcc $(SOURCE) -o build/mock $(CFLAGS) $(LDFLAGS)
