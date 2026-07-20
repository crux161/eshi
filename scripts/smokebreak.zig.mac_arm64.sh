#!/bin/sh

make BUILD_DIR=build-make -j$(sysctl -n hw.ncpu)
zig build -p build-zig

hyperfine -i \
  './build-make/warp --res 320x180' \
  './build-zig/bin/warp --res 320x180'
