#!/bin/bash
# Spike build of three libretro cores and standalone DSperate for tg5040 (TrimUI Brick). Runs inside the
# ghcr.io/loveretro/tg5040-toolchain container with this folder mounted at /root/ds-psp.
# Usage (from the repo root on the Mac):
#   docker run --rm -v "$PWD/workspace":/root/workspace -v "$PWD/spikes/ds-psp":/root/ds-psp \
#     ghcr.io/loveretro/tg5040-toolchain:latest /bin/bash -c \
#     '. ~/.bashrc && export PLATFORM=tg5040 UNION_PLATFORM=tg5040 && /root/ds-psp/build.sh melonds desmume2015 ppsspp'
set -e
cd /root/ds-psp
mkdir -p out
SRC=/root/ds-psp/src
PROCS=-j$(nproc)
# The same CPU flags the shipped tg5040 cores use (workspace/tg5040/cores/patches/*.patch).
CPU="-mcpu=cortex-a53 -mtune=cortex-a53 -fomit-frame-pointer"

build_melonds() {
	cd "$SRC/melonds"
	# platform=unix with ARCH=arm64 turns on the AArch64 JIT and the threaded software renderer.
	# OpenGL stays off (HAVE_OPENGL=0): minarch has no hardware render path.
	CFLAGS="$CPU" CXXFLAGS="$CPU" make $PROCS platform=unix ARCH=arm64 \
		CC=${CROSS_COMPILE}gcc CXX=${CROSS_COMPILE}g++ AR=${CROSS_COMPILE}ar
	${CROSS_COMPILE}strip -o /root/ds-psp/out/melonds_libretro.so melonds_libretro.so
}

build_desmume2015() {
	cd "$SRC/desmume2015/desmume"
	# DeSmuME 2015 has no AArch64 dynarec, so both JIT switches are off (interpreter only).
	CFLAGS="$CPU" CXXFLAGS="$CPU" make $PROCS -f Makefile.libretro platform=unix \
		DESMUME_JIT=0 DESMUME_JIT_ARM=0 \
		CC=${CROSS_COMPILE}gcc CXX=${CROSS_COMPILE}g++ AR=${CROSS_COMPILE}ar
	${CROSS_COMPILE}strip -o /root/ds-psp/out/desmume2015_libretro.so desmume2015_libretro.so
}

build_ppsspp() {
	cd "$SRC/ppsspp"
	# The toolchain's GCC 8.3 needs two small source patches (see README.md):
	# ppsspp-gcc8-copysign.patch avoids an internal compiler error on copysignf in GPU/Software/GEMath.h,
	# and ppsspp-gcc8-ctad.patch avoids class template deduction that GCC 8 cannot do in Core/Screenshot.cpp.
	for p in /root/ds-psp/patches/ppsspp-*.patch; do
		git apply --check "$p" 2>/dev/null && git apply "$p"
	done
	# The container's CMAKE_TOOLCHAIN_FILE overwrites CMAKE_CXX_FLAGS, so the CPU flags go in the
	# Release flags instead.
	cmake -S . -B build-tg5040 \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
		-DCMAKE_C_COMPILER=${CROSS_COMPILE}gcc -DCMAKE_CXX_COMPILER=${CROSS_COMPILE}g++ \
		-DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG $CPU" -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG $CPU" \
		-DLIBRETRO=ON -DUSING_GLES2=ON -DUSING_EGL=OFF -DUSING_X11_VULKAN=OFF -DUSE_WAYLAND_WSI=OFF \
		-DUSE_DISCORD=OFF -DUSE_MINIUPNPC=OFF -DUSE_FFMPEG=ON -DUSE_SYSTEM_FFMPEG=OFF \
		$PPSSPP_EXTRA
	cmake --build build-tg5040 --target ppsspp_libretro -- $PROCS
	${CROSS_COMPILE}strip -o /root/ds-psp/out/ppsspp_libretro.so build-tg5040/lib/ppsspp_libretro.so
}

build_dsperate() {
	# Standalone DSperate (not a libretro core), at the tag recorded in README.md.
	cd "$SRC/dsperate"
	# dsperate-gcc8-neon-scale-row-grid.patch: GCC 8 miscompiles one NEON kernel (LCD grid
	# filter only), from josegonzalez/minui-nintendo-dsperate-pak (MIT).
	# dsperate-stylus-tap-under-hold.patch: lets A tap the touch screen while R2 is held
	# and stay the DS A button otherwise (Bramley's design).
	# dsperate-bramley-power.patch: the power key, sleep and power off inside
	# DSperate, following Bramley's PWR_sleep() (approved 2026-11-05).
	# Already-applied patches fail the check and are skipped.
	for p in /root/ds-psp/patches/dsperate-gcc8-neon-scale-row-grid.patch \
		/root/ds-psp/patches/dsperate-stylus-tap-under-hold.patch \
		/root/ds-psp/patches/dsperate-bramley-power.patch; do
		git apply --check "$p" 2>/dev/null && git apply "$p"
	done
	# The sysroot's Vulkan headers are too old for DSperate's Vulkan code (no
	# VK_API_VERSION_1_2), so newer Khronos headers come from src/vulkan-headers.
	# Nothing links libvulkan: DSperate dlopens it, and the Brick has /usr/lib/libvulkan.so.1.
	local S=/opt/aarch64-nextui-linux-gnu/aarch64-nextui-linux-gnu/libc
	export PKG_CONFIG_PATH="$S/usr/lib/pkgconfig" PKG_CONFIG_SYSROOT_DIR="$S"
	# DSPERATE_TUNE puts the CPU flags on every compile and on the LTO link, so the
	# toolchain file's CMAKE_CXX_FLAGS cannot drop them. libpthread goes last on the
	# link line, where --as-needed keeps it (glibc 2.28 still has a separate libpthread).
	# libstdc++ and libgcc are linked statically, as the NextUI DSperate pak does.
	cmake -S . -B build-tg5040 -G Ninja \
		-DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE" \
		-DCMAKE_BUILD_TYPE=Release \
		-DDSPERATE_TUNE="$CPU" \
		-DDSPERATE_JIT=ON -DDSPERATE_NEON=ON -DDSPERATE_SDL=ON -DDSPERATE_WAYLAND=OFF \
		-DDSPERATE_HEADLESS=OFF -DDSPERATE_TESTS=OFF -DDSPERATE_CHEEVOS=ON -DDSPERATE_NET=ON \
		-DDSPERATE_GLES=ON -DDSPERATE_VULKAN=ON -DDSPERATE_PGO=OFF \
		-DDSPERATE_VULKAN_INCLUDE="$SRC/vulkan-headers/include" \
		-DCMAKE_C_FLAGS="-I$CROSS_ROOT/include/ -pthread" \
		-DCMAKE_CXX_FLAGS="-I$CROSS_ROOT/include/ -pthread" \
		-DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc" \
		-DCMAKE_CXX_STANDARD_LIBRARIES="-Wl,--push-state,--no-as-needed -lpthread -Wl,--pop-state"
	cmake --build build-tg5040 --target dsperate -- $PROCS
	${CROSS_COMPILE}strip --strip-unneeded -o /root/ds-psp/out/dsperate build-tg5040/src/frontend/sdl/dsperate
}

for core in "$@"; do
	echo "=== $core ==="
	build_$core
done
ls -l /root/ds-psp/out
