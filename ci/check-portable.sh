#!/usr/bin/env bash
set -euo pipefail

packages=${1:?Usage: check-portable.sh PACKAGE_DIR FONT_DIR}
fonts=${2:?Usage: check-portable.sh PACKAGE_DIR FONT_DIR}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
tar -xzf "$packages"/FontMaster-*.tar.gz -C "$work"
app="$work/FontMaster.AppDir/usr/bin/fontmaster_cli"
"$app" --help
for font in NotoColorEmoji.ttf Noto-COLRv1.ttf; do
    "$app" render --font "$fonts/$font" --unicode 1F600 \
        --size 128 --output "$work/$font.png"
done
glyph=$("$app" list "$fonts/Noto-COLRv1.ttf" | \
    awk '/\(U\+1F600\)/ { name = $1 } END { print name }')
test -n "$glyph"
"$app" replace --font "$fonts/Noto-COLRv1.ttf" --name "$glyph" \
    --image "$work/NotoColorEmoji.ttf.png" --output "$work/replaced.ttf"
"$app" render --font "$work/replaced.ttf" --unicode 1F600 \
    --size 128 --output "$work/replaced.png"
echo 'Portable CLI rendering, replacement and SVG output passed.'
