#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${ARDUINOJSON_INCLUDE:?Set this to the ArduinoJson/src directory}"
mkdir -p build/tests
c++ -std=c++17 -Wall -Wextra -Werror -I tests/input_fakes -I LEAP/src tests/test_input.cpp -o build/tests/input
build/tests/input
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_aircraft_map.cpp -o build/tests/aircraft-map
build/tests/aircraft-map
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_storage_recovery.cpp -o build/tests/storage-recovery
build/tests/storage-recovery
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src -I "$ARDUINOJSON_INCLUDE" tests/test_core.cpp -o build/tests/core
build/tests/core "${@}"
python3 tests/test_partitions.py
