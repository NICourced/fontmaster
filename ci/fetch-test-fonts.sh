#!/usr/bin/env bash
set -euo pipefail

destination=${1:?Usage: fetch-test-fonts.sh DIRECTORY}
mkdir -p "$destination"
revision=8998f5dd683424a73e2314a8c1f1e359c19e8742
for name in NotoColorEmoji.ttf NotoColorEmoji-emojicompat.ttf \
    NotoColorEmoji-flagsonly.ttf NotoColorEmoji-noflags.ttf \
    NotoColorEmoji_WindowsCompatible.ttf Noto-COLRv1.ttf \
    Noto-COLRv1-emojicompat.ttf Noto-COLRv1-noflags.ttf LICENSE; do
    curl --fail --location --retry 3 --retry-all-errors \
        "https://raw.githubusercontent.com/googlefonts/noto-emoji/$revision/fonts/$name" \
        --output "$destination/$name"
done
