#!/bin/bash
# Yes this is a play on words with stream and river.

meson setup build
ninja -C build

river -c river -c ./build/stream