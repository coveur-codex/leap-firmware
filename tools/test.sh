#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${ARDUINOJSON_INCLUDE:?Set this to the ArduinoJson/src directory}"
mkdir -p build/tests
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_weather_icon.cpp -o build/tests/weather-icon
build/tests/weather-icon
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_math_quiz.cpp -o build/tests/math-quiz
build/tests/math-quiz
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src -I "$ARDUINOJSON_INCLUDE" tests/test_quiz_questions.cpp -o build/tests/quiz-questions
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src -I "$ARDUINOJSON_INCLUDE" tests/test_pet_assets.cpp -o build/tests/pet-assets
build/tests/pet-assets
build/tests/quiz-questions
c++ -std=c++17 -Wall -Wextra -Werror -I tests/input_fakes -I LEAP/src tests/test_input.cpp -o build/tests/input
build/tests/input
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_games.cpp -o build/tests/games
c++ -std=c++17 -Wall -Wextra -Werror -I tests/game_fakes -I tests/input_fakes -I "$ARDUINOJSON_INCLUDE" -I LEAP/src tests/test_game_lifecycle.cpp LEAP/src/Games.cpp -o build/tests/game-lifecycle
build/tests/game-lifecycle
build/tests/games
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_aircraft_map.cpp -o build/tests/aircraft-map
build/tests/aircraft-map
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_storage_recovery.cpp -o build/tests/storage-recovery
build/tests/storage-recovery
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src -I "$ARDUINOJSON_INCLUDE" tests/test_core.cpp -o build/tests/core
build/tests/core "${@}"
python3 tests/test_partitions.py
