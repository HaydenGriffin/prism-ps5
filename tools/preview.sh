#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
source tools/ninja-build.sh
cxx=$(command -v "${HOST_CXX:-clang++}")
ninja_begin "$root/build/preview-build/build.ninja"
sources=(src/prism/model.cpp src/prism/remote.cpp src/prism/application.cpp src/prism/view.cpp src/core/input.cpp src/core/save_file.cpp host/platform_host.cpp host/preview.cpp src/third_party/stb_image.cpp src/gfx/*.cpp src/ui/*.cpp src/ui/components/*.cpp)
objects=()
for source in "${sources[@]}"; do
 object="$root/build/preview-build/${source//\//_}.o"
 ninja_inputs=("$root/$source" "$cxx")
 ninja_edge CXX "$object" "${compiler_cache[@]}" "$cxx" -std=c++20 -O2 -Wall -Wextra -DGL_GLEXT_PROTOTYPES=1 -I"$root/src" -MD -MF "$object.d" -c "$root/$source" -o "$object"
 objects+=("$object")
done
ninja_inputs=("${objects[@]}")
ninja_edge LINK "$root/build/preview-build/prism-preview" "$cxx" "${objects[@]}" -lEGL -lGL -pthread -o "$root/build/preview-build/prism-preview"
ninja_run
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe PRISM_PREVIEW_DATA="${PRISM_PREVIEW_DATA:-}" build/preview-build/prism-preview
