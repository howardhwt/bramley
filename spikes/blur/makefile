# make mac    builds build/blur-mac and runs nothing; ./build/blur-mac opens a 1024x768 window.
# make        builds build/Blur Spike.pak for the Brick. Copy it to SD:/Tools/tg5040/Blur Spike.pak/

IMAGE = ghcr.io/loveretro/tg5040-toolchain:latest
CONTAINER_RUNTIME ?= $(shell command -v docker 2>/dev/null || command -v podman 2>/dev/null)
CFLAGS = -O2 -Wall -Wextra -Wno-unused-parameter

PAK = build/Blur Spike.pak

.PHONY: all mac clean

all: build/.pak

mac: build/blur-mac

build/blur-mac: blur.c
	mkdir -p build
	cc $(CFLAGS) -o $@ blur.c $$(sdl2-config --cflags --libs) -framework OpenGL

build/blur: blur.c
	mkdir -p build
	$(CONTAINER_RUNTIME) run --rm -v "$(CURDIR)":/spike $(IMAGE) /bin/bash -c \
		'. ~/.bashrc && $${CROSS_COMPILE}gcc $(CFLAGS) -mcpu=cortex-a53 -DBLUR_DEVICE -o /spike/build/blur /spike/blur.c \
		$$(pkg-config --cflags --libs sdl2 glesv2) -lm'

build/.pak: build/blur launch.sh
	mkdir -p "$(PAK)"
	cp build/blur launch.sh "$(PAK)/"
	chmod +x "$(PAK)/blur" "$(PAK)/launch.sh"
	file "$(PAK)/blur"
	touch $@

clean:
	rm -rf build
