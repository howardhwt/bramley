#!/bin/bash
# Reproduces the GCC 8.3 internal compiler error on PPSSPP's software rasterizer with several flag sets.
cd /root/ds-psp/src/ppsspp/build-tg5040
F=$(grep '^CXX_DEFINES\|^CXX_INCLUDES' GPU/CMakeFiles/GPU.dir/flags.make | sed 's/^[A-Z_]* = //' | tr '\n' ' ')
BASE="-std=gnu++17 -fPIC -fno-strict-aliasing -fno-math-errno -w"
for opt in "-O3 -mcpu=cortex-a53 -mtune=cortex-a53"; do for f in Rasterizer TransformUnit; do
  if ${CROSS_COMPILE}g++ $F $BASE $opt -c ../GPU/Software/$f.cpp -o /tmp/r.o 2>/tmp/err; then echo "OK   $f $opt"; else echo "FAIL $f $opt: $(grep -m1 'internal compiler\|error' /tmp/err)"; fi
done; done
