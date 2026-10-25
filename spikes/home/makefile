# make mac    builds build/home-mac; ./build/home-mac opens a 1024x768 window.
# make        builds build/Home Spike.pak for the Brick. Copy it to SD:/Tools/tg5040/Home Spike.pak/

IMAGE = ghcr.io/loveretro/tg5040-toolchain:latest
CONTAINER_RUNTIME ?= $(shell command -v docker 2>/dev/null || command -v podman 2>/dev/null)
CFLAGS = -O2 -Wall -Wextra -Wno-unused-parameter

PAK = build/Home Spike.pak

.PHONY: all mac clean

all: build/.pak

mac: build/home-mac

build/home-mac: home.c
	mkdir -p build
	cc $(CFLAGS) -o $@ home.c $$(sdl2-config --cflags --libs) -framework OpenGL

build/home: home.c
	mkdir -p build
	$(CONTAINER_RUNTIME) run --rm -v "$(CURDIR)":/spike $(IMAGE) /bin/bash -c \
		'. ~/.bashrc && $${CROSS_COMPILE}gcc $(CFLAGS) -mcpu=cortex-a53 -DHOME_DEVICE -o /spike/build/home /spike/home.c \
		$$(pkg-config --cflags --libs sdl2 glesv2) -lm'

build/.pak: build/home launch.sh
	mkdir -p "$(PAK)"
	cp build/home launch.sh "$(PAK)/"
	chmod +x "$(PAK)/home" "$(PAK)/launch.sh"
	file "$(PAK)/home"
	touch $@

clean:
	rm -rf build
