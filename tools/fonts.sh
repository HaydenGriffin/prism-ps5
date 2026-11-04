#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/host assets/fonts
# The Lexend files have had their overlapping outlines merged, since the distance field baker would
# otherwise draw those inner edges as seams inside the digits:
#   python -m fontTools.ttLib.removeOverlaps in.ttf out.ttf    (fonttools with skia-pathops)
${HOST_CXX:-clang++} -std=c++20 -O2 -w tools/font-baker/bake_font.cpp -o build/host/bake_font
for face in atkinson_regular atkinson_semibold lexend_regular lexend_semibold; do
    build/host/bake_font "third_party/fonts/$face.ttf" "assets/fonts/${face//_/-}.huifont" 56 8 2048 european
done
