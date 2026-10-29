###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
	$(error please specify PLATFORM, eg. PLATFORM=tg5040 make)
endif

ifeq (,$(CROSS_COMPILE))
	$(error missing CROSS_COMPILE for this toolchain)
endif

###########################################################

include ../../$(PLATFORM)/platform/makefile.env
SDL?=SDL

###########################################################

# The launcher also carries Settings (../settings), which it opens as a screen of its own
# instead of starting it as a separate program.
TARGET = launcher
S = ../settings
INCDIR = -I. -Itortos -I$(S) -I../common/ -I../../$(PLATFORM)/platform/
SOURCE = $(TARGET).c lgfx.c lwallpaper.c ldata.c lactivity.c lfiles.c lradio.c lmuse.c lmusic.c tortos/muselib.c tortos/musequeue.c \
	../common/scaler.c ../common/utils.c ../common/config.c ../common/api.c ../../$(PLATFORM)/platform/platform.c \
	../common/palette.c ../common/http.c ../common/ra_auth.c ../common/ra_offline.c ../common/ra_sync.c ../common/ra_event_queue.c $(S)/bramley_battery.c bramley_update.c
CXXSOURCE = $(S)/settings.cpp $(S)/menu.cpp $(S)/colorpickermenu.cpp $(S)/palettemenu.cpp $(S)/fnbuttonmenu.cpp \
	$(S)/wifimenu.cpp $(S)/btmenu.cpp $(S)/keyboardprompt.cpp $(S)/bramley_settings.cpp $(S)/bramley_pages.cpp

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
# the platform's CFLAGS hold its include paths and defines, which C++ needs too
FLAGS := $(CFLAGS) $(OPT) $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -DLAUNCHER_VERSION=\"$(shell cat VERSION)\" -DBRAMLEY_HOSTED
CFLAGS := $(FLAGS) -std=gnu99
CXXFLAGS := $(FLAGS) -std=c++17
LDFLAGS += -lmsettings -lcrypto -lsqlite3
ifneq (,$(filter $(PLATFORM),tg5040 tg5050 h700))
CXXFLAGS += -DHAS_BTAGENT $$(pkg-config --cflags gio-2.0 glib-2.0)
CXXSOURCE += $(S)/btagent.cpp
LDFLAGS += $$(pkg-config --libs gio-2.0 glib-2.0)
endif

PRODUCT = build/$(PLATFORM)/$(TARGET).elf
OBJ = build/$(PLATFORM)/obj

all: $(PREFIX_LOCAL)/include/msettings.h
	mkdir -p $(OBJ)
	rm -f $(OBJ)/*.o
	for f in $(SOURCE); do $(CC) -c $$f -o $(OBJ)/$$(echo $$f | tr '/.' '__').o $(CFLAGS) || exit 1; done
	for f in $(CXXSOURCE); do $(CXX) -c $$f -o $(OBJ)/$$(echo $$f | tr '/.' '__').o $(CXXFLAGS) || exit 1; done
	$(CXX) $(OBJ)/*.o -o $(PRODUCT) $(CXXFLAGS) $(LDFLAGS) -lstdc++
clean:
	rm -f $(PRODUCT)
	rm -rf $(OBJ)

$(PREFIX_LOCAL)/include/msettings.h:
	cd ../../$(PLATFORM)/libmsettings && make
