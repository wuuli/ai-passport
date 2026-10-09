#!/usr/bin/env bash
# Fork application checks; deliberately independent of the generic board gate.
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"
export PYTHONDONTWRITEBYTECODE=1

test_dir="$(mktemp -d /tmp/ai-passport-game-tests.XXXXXX)"
trap 'rm -rf -- "${test_dir}"' EXIT

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_duel_clock.c main/duel_clock.c -lm \
    -o "${test_dir}/test_duel_clock"
"${test_dir}/test_duel_clock"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_corridor_game.c main/corridor_game.c -lm \
    -o "${test_dir}/test_corridor_game"
"${test_dir}/test_corridor_game"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
    -Icomponents/bsp/include -Itests/stubs/corridor_demo \
    tests/test_corridor_demo.c main/corridor_game.c main/corridor_sound.c -lm \
    -o "${test_dir}/test_corridor_demo"
"${test_dir}/test_corridor_demo"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_corridor_render.c main/corridor_render.c main/corridor_game.c -lm \
    -o "${test_dir}/test_corridor_render"
"${test_dir}/test_corridor_render"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_duel_sound.c main/duel_sound.c \
    -o "${test_dir}/test_duel_sound"
"${test_dir}/test_duel_sound"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_corridor_sound.c main/corridor_sound.c main/corridor_game.c -lm \
    -o "${test_dir}/test_corridor_sound"
"${test_dir}/test_corridor_sound"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
    -Itests/duel_demo/stubs -Icomponents/bsp/include \
    tests/test_demo_duel.c main/demo_duel.c main/duel_clock.c -lm \
    -o "${test_dir}/test_demo_duel"
"${test_dir}/test_demo_duel"
python3 tests/test_duel_assets.py
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/game_main_stubs -Imain \
    tests/test_game_main.c main/demo_navigation.c \
    -o "${test_dir}/test_game_main"
"${test_dir}/test_game_main"
python3 tools/build_corridor_web.py --check
node tests/test_corridor_web.mjs
node tests/test_corridor_audio.mjs
python3 tools/build_duel_web.py --check
node tests/test_duel_web.mjs
node tests/test_duel_preview.mjs
PYTHONDONTWRITEBYTECODE=1 python3 tests/test_web_build_manifest.py
PYTHONDONTWRITEBYTECODE=1 python3 tests/test_game_preview_server.py
echo "Game tests: PASS"
