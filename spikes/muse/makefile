# Builds build/Muse Spike.pak from the TortOS v1.6.0 release and the vendored musectl.c.
# Copy the result to SD:/Tools/tg5040/Muse Spike.pak/

TORTOS_TAG = v1.6.0
TORTOS_ZIP = TortOS-$(TORTOS_TAG).zip
TORTOS_SHA256 = 54bd2cb7552fd3c7e27609f6dd6549d356cf06c5e92be18921fbc360c51c076a
TORTOS_URL = https://github.com/ericreinsmidt/TortOS/releases/download/$(TORTOS_TAG)/$(TORTOS_ZIP)

IMAGE = ghcr.io/loveretro/tg5040-toolchain:latest
CONTAINER_RUNTIME ?= $(shell command -v docker 2>/dev/null || command -v podman 2>/dev/null)

PAK = build/Muse Spike.pak

.PHONY: all clean

all: build/.pak

build/$(TORTOS_ZIP):
	mkdir -p build
	curl -fL -o $@.part $(TORTOS_URL)
	echo "$(TORTOS_SHA256)  $@.part" | shasum -a 256 -c -
	mv $@.part $@

build/muse: build/$(TORTOS_ZIP)
	unzip -o -j $< TortOS/muse -d build
	touch $@

build/musectl: src/musectl.c
	mkdir -p build
	$(CONTAINER_RUNTIME) run --rm -v "$(CURDIR)":/spike $(IMAGE) /bin/bash -c \
		'. ~/.bashrc && $${CROSS_COMPILE}gcc -O2 -Wall -o /spike/build/musectl /spike/src/musectl.c'

build/.pak: build/muse build/musectl launch.sh
	mkdir -p "$(PAK)"
	cp build/muse build/musectl launch.sh "$(PAK)/"
	chmod +x "$(PAK)/muse" "$(PAK)/musectl" "$(PAK)/launch.sh"
	file "$(PAK)/muse" "$(PAK)/musectl"
	touch $@

clean:
	rm -rf build
