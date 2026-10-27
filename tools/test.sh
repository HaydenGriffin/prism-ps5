#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
source tools/ninja-build.sh
cxx=$(command -v "${HOST_CXX:-clang++}")
ninja_begin "$root/build/tests/build.ninja"
sources=(src/prism/model.cpp src/prism/client.cpp src/prism/remote.cpp src/prism/resources.cpp src/prism/application.cpp src/prism/view.cpp src/afterglow/ma_socket.cpp src/afterglow/artwork_fetch.cpp src/third_party/stb_image.cpp src/core/input.cpp src/core/save_file.cpp src/platform/ps5/keyboard.cpp host/platform_host.cpp src/gfx/font.cpp src/gfx/draw_list.cpp src/gfx/triangulate.cpp src/ui/theme.cpp src/ui/widgets.cpp src/ui/pixel_font.cpp src/ui/glyphs.cpp src/ui/components/component.cpp src/ui/components/card.cpp src/ui/components/grid.cpp src/ui/components/list.cpp tests/tests.cpp)
objects=()
for source in "${sources[@]}"; do
  object="$root/build/tests/${source//\//_}.o"
  ninja_inputs=("$root/$source" "$cxx")
  ninja_edge CXX "$object" "${compiler_cache[@]}" "$cxx" -std=c++20 -O1 -g -Wall -Wextra -Werror -Wno-unused-parameter -fsanitize=address,undefined -fno-omit-frame-pointer -DJSON_NOEXCEPTION=1 -I"$root/src" -MD -MF "$object.d" -c "$root/$source" -o "$object"
  objects+=("$object")
done
ninja_inputs=("${objects[@]}")
ninja_edge LINK "$root/build/tests/prism-tests" "$cxx" "${objects[@]}" -fsanitize=address,undefined -pthread -o "$root/build/tests/prism-tests"
ninja_run
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 build/tests/prism-tests
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 python3 tests/integration.py
