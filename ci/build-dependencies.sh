#!/usr/bin/env bash
set -euo pipefail

prefix=$(realpath -m "${1:?Usage: build-dependencies.sh PREFIX WORK_DIR}")
work=$(realpath -m "${2:?Usage: build-dependencies.sh PREFIX WORK_DIR}")
jobs=${CMAKE_BUILD_PARALLEL_LEVEL:-2}
mkdir -p "$prefix" "$work"

download() {
    curl --fail --location --retry 3 --retry-all-errors "$1" --output "$2"
    printf '%s  %s\n' "$3" "$2" | sha256sum --check --strict
}

download https://github.com/harfbuzz/harfbuzz/releases/download/15.1.0/harfbuzz-15.1.0.tar.xz \
    "$work/harfbuzz.tar.xz" 0fc7c565443a5f4be7da65ac5ee5bd737d9b158dc908708b7689a0a5796b0c8d
tar -xJf "$work/harfbuzz.tar.xz" -C "$work"
meson setup "$work/harfbuzz-build" "$work/harfbuzz-15.1.0" \
    --prefix="$prefix" --libdir=lib --buildtype=release --default-library=shared \
    -Dglib=enabled -Dfreetype=enabled -Dgobject=disabled -Dicu=disabled \
    -Dcairo=disabled -Dchafa=disabled -Ddocs=disabled -Dintrospection=disabled \
    -Dtests=disabled -Dutilities=disabled -Dsubset=enabled -Draster=enabled \
    -Dvector=disabled -Dpng=enabled -Dzlib=enabled
meson compile -C "$work/harfbuzz-build" -j "$jobs"
meson install -C "$work/harfbuzz-build"

download https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.1.1/libjpeg-turbo-3.1.1.tar.gz \
    "$work/libjpeg-turbo.tar.gz" aadc97ea91f6ef078b0ae3a62bba69e008d9a7db19b34e4ac973b19b71b4217c
tar -xzf "$work/libjpeg-turbo.tar.gz" -C "$work"
cmake -S "$work/libjpeg-turbo-3.1.1" -B "$work/jpeg-build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DCMAKE_INSTALL_LIBDIR=lib -DENABLE_STATIC=OFF -DWITH_TOOLS=OFF -DWITH_TESTS=OFF
cmake --build "$work/jpeg-build" --parallel "$jobs"
cmake --install "$work/jpeg-build"

mkdir -p "$prefix/share/fontmaster-dependencies"
cp "$work/harfbuzz-15.1.0/COPYING" "$prefix/share/fontmaster-dependencies/harfbuzz.txt"
cp "$work/libjpeg-turbo-3.1.1/LICENSE.md" "$prefix/share/fontmaster-dependencies/libjpeg-turbo.txt"
cp "$work/libjpeg-turbo-3.1.1/README.ijg" "$prefix/share/fontmaster-dependencies/README.ijg"
