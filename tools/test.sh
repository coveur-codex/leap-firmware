#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${ARDUINOJSON_INCLUDE:?Set this to the ArduinoJson/src directory}"
mkdir -p build/tests
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src -I "$ARDUINOJSON_INCLUDE" tests/test_core.cpp -o build/tests/core
build/tests/core "${@}"
python3 tests/test_partitions.py
