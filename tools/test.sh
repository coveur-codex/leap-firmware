#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${ARDUINOJSON_INCLUDE:?Set this to the ArduinoJson/src directory}"
mkdir -p build/tests
c++ -std=c++17 -Wall -Wextra -Werror -I tests/storage_fakes -I LEAP/src tests/test_device_config.cpp -o build/tests/device-config
build/tests/device-config
for mode in 0 1; do
  c++ -std=c++17 -Wall -Wextra -Werror -DLEAP_PROVISION_DEVICE=$mode -I tests/storage_fakes -I "$ARDUINOJSON_INCLUDE" -I LEAP/src tests/test_device_config_boot.cpp LEAP/src/DeviceConfig.cpp -o build/tests/device-config-boot-$mode
  build/tests/device-config-boot-$mode
done
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_crab_journey.cpp -o build/tests/crab-journey
build/tests/crab-journey
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src -I "$ARDUINOJSON_INCLUDE" tests/test_game_selection.cpp -o build/tests/game-selection
build/tests/game-selection
c++ -std=c++17 -Wall -Wextra -Werror -I tests/storage_fakes -I "$ARDUINOJSON_INCLUDE" -I LEAP/src tests/test_storage_snapshot.cpp LEAP/src/Storage.cpp -o build/tests/storage-snapshot
build/tests/storage-snapshot
c++ -std=c++17 -Wall -Wextra -Werror -I tests/storage_fakes -I "$ARDUINOJSON_INCLUDE" -I LEAP/src tests/test_memory_usage.cpp LEAP/src/Storage.cpp -o build/tests/memory-usage
build/tests/memory-usage
c++ -std=c++17 -Wall -Wextra -Werror -I tests/storage_fakes -I "$ARDUINOJSON_INCLUDE" -I LEAP/src tests/test_quiz_tracking.cpp LEAP/src/QuizTracking.cpp LEAP/src/Storage.cpp -o build/tests/quiz-tracking
build/tests/quiz-tracking
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_maze.cpp -o build/tests/maze
build/tests/maze
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_kitchen.cpp -o build/tests/kitchen
c++ -std=c++17 -Wall -Wextra -Werror -I tests/input_fakes -I tests/game_fakes -I LEAP/src tests/test_kitchen_editor.cpp LEAP/src/KitchenEditor.cpp -o build/tests/kitchen-editor
build/tests/kitchen-editor
build/tests/kitchen
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
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_motion_calibration.cpp -o build/tests/motion-calibration
build/tests/motion-calibration
c++ -std=c++17 -Wall -Wextra -Werror -I tests/motion_fakes -I tests/storage_fakes -I tests/input_fakes -I "$ARDUINOJSON_INCLUDE" -I LEAP/src tests/test_motion.cpp LEAP/src/Motion.cpp -o build/tests/motion
build/tests/motion
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_games.cpp -o build/tests/games
c++ -std=c++17 -Wall -Wextra -Werror -I tests/game_fakes -I tests/input_fakes -I "$ARDUINOJSON_INCLUDE" -I LEAP/src tests/test_game_lifecycle.cpp LEAP/src/Games.cpp LEAP/src/CrabJourneyDraw.cpp LEAP/src/KitchenEditor.cpp -o build/tests/game-lifecycle
build/tests/game-lifecycle
c++ -std=c++17 -Wall -Wextra -Werror -I tests/game_fakes -I tests/input_fakes -I "$ARDUINOJSON_INCLUDE" -I LEAP/src tests/test_chill.cpp LEAP/src/Chill.cpp -o build/tests/chill
build/tests/chill
build/tests/games
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_connect_four.cpp -o build/tests/connect-four
build/tests/connect-four
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_aircraft_map.cpp -o build/tests/aircraft-map
build/tests/aircraft-map
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src tests/test_storage_recovery.cpp -o build/tests/storage-recovery
build/tests/storage-recovery
c++ -std=c++17 -Wall -Wextra -Werror -I LEAP/src -I "$ARDUINOJSON_INCLUDE" tests/test_core.cpp -o build/tests/core
build/tests/core "${@}"
python3 tests/test_partitions.py
