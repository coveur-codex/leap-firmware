#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${PNGDEC_SRC:?Set this to the pinned PNGdec 1.1.6 src directory}"
mkdir -p build/tests/png/objects
python3 tests/generate_png_fixtures.py build/tests/png
for source in "$PNGDEC_SRC"/*.c; do
  cc -D__LINUX__ -O1 -c "$source" -o "build/tests/png/objects/$(basename "$source" .c).o"
done
# Use exactly the same global options as Arduino, for caller AND library source.
c++ -std=c++17 -D__LINUX__ @LEAP/build_opt.h -O1 -I "$PNGDEC_SRC" \
  tests/test_png_decoder.cpp "$PNGDEC_SRC/PNGdec.cpp" build/tests/png/objects/*.o \
  -o build/tests/png/decoder
build/tests/png/decoder build/tests/png/*.png

c++ -std=c++17 -D__LINUX__ -Wall -Wextra -Werror -I LEAP/src -I "$PNGDEC_SRC" tests/test_media_mask.cpp -o build/tests/png/mask
build/tests/png/mask

: "${ARDUINOJSON_INCLUDE:?Set this to ArduinoJson/src for the Media integration test}"
c++ -std=c++17 -D__LINUX__ @LEAP/build_opt.h -O1 -I tests/media_fakes -I tests/game_fakes -I "$ARDUINOJSON_INCLUDE" -I "$PNGDEC_SRC" -I LEAP/src tests/test_media_render.cpp LEAP/src/Media.cpp "$PNGDEC_SRC/PNGdec.cpp" build/tests/png/objects/*.o -o build/tests/png/render
build/tests/png/render build/tests/png
