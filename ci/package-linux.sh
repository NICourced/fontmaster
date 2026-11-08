#!/usr/bin/env bash
set -euo pipefail

build=$(realpath "${1:?Usage: package-linux.sh BUILD_DIR OUTPUT_DIR DEPENDENCY_PREFIX}")
output=$(realpath -m "${2:?Usage: package-linux.sh BUILD_DIR OUTPUT_DIR DEPENDENCY_PREFIX}")
prefix=$(realpath "${3:?Usage: package-linux.sh BUILD_DIR OUTPUT_DIR DEPENDENCY_PREFIX}")
test_fonts=${4:-}
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
version=$(cat "$build/fontmaster-version.txt")
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]
tools="$build/release-tools"
stage="$build/release-stage"
appdir="$stage/FontMaster.AppDir"
mkdir -p "$tools" "$output"
if [[ -e $stage ]]; then
    echo "Packaging directory already exists: $stage" >&2
    exit 1
fi
mkdir -p "$appdir"

download() {
    curl --fail --location --retry 3 --retry-all-errors "$1" --output "$2"
    printf '%s  %s\n' "$3" "$2" | sha256sum --check --strict
    chmod +x "$2"
}
download https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage \
    "$tools/linuxdeploy-x86_64.AppImage" c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
download https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage \
    "$tools/linuxdeploy-plugin-qt-x86_64.AppImage" 15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724

DESTDIR="$appdir" cmake --install "$build" --prefix /usr
export APPIMAGE_EXTRACT_AND_RUN=1
export ARCH=x86_64
export VERSION="$version"
export EXTRA_PLATFORM_PLUGINS=libqoffscreen.so
export OUTPUT="$output/FontMaster-$version-linux-x86_64.AppImage"
export LD_LIBRARY_PATH="$prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
libraries=()
while read -r name path; do
    case "$name" in
        libfontmaster.so.*|libc.so.*|libm.so.*|libpthread.so.*|libdl.so.*|librt.so.*|libresolv.so.*|libstdc++.so.*|libgcc_s.so.*)
            continue ;;
    esac
    libraries+=(--library "$path")
done < <(ldd "$appdir/usr/bin/fontmaster_cli" | awk '$2 == "=>" && $3 ~ /^\// { print $1, $3 }')
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" \
    --executable "$appdir/usr/bin/fontmaster_qt" \
    --executable "$appdir/usr/bin/fontmaster_cli" \
    "${libraries[@]}" \
    --desktop-file "$source_dir/packaging/fontmaster.desktop" \
    --icon-file "$source_dir/packaging/fontmaster.svg" --plugin qt

notices="$appdir/usr/share/doc/fontmaster/third-party"
mkdir -p "$notices"
cp -a "$prefix/share/fontmaster-dependencies/." "$notices/"
cp -a /usr/share/common-licenses "$notices/"
for library in "$appdir"/usr/lib/*.so*; do
    name=$(basename "$library")
    system_library=$(ldconfig -p | awk -v name="$name" '$1 == name && !found { print $NF; found = 1 }')
    [[ -n $system_library ]] || continue
    package=$(dpkg-query --search "$(readlink -f "$system_library")" 2>/dev/null | head -n 1 | cut -d: -f1 || true)
    if [[ -n $package && -f /usr/share/doc/$package/copyright ]]; then
        cp -L "/usr/share/doc/$package/copyright" "$notices/$package.copyright"
    fi
done

"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" --output appimage
test -s "$OUTPUT"
test -x "$appdir/usr/bin/fontmaster_cli"
test -x "$appdir/usr/bin/fontmaster_qt"
env -u LD_LIBRARY_PATH "$appdir/usr/bin/fontmaster_cli" --help
if [[ -n $test_fonts ]]; then
    for font in NotoColorEmoji.ttf Noto-COLRv1.ttf; do
        env -u LD_LIBRARY_PATH "$appdir/usr/bin/fontmaster_cli" render \
            --font "$test_fonts/$font" --unicode 1F600 --size 128 \
            --output "$build/package-$font.png"
    done
fi
set +e
env -u LD_LIBRARY_PATH QT_QPA_PLATFORM=offscreen \
    timeout 3s "$appdir/AppRun" >"$build/qt-package-smoke.log" 2>&1
result=$?
set -e
if [[ $result != 124 ]]; then
    cat "$build/qt-package-smoke.log" >&2
    echo "Qt package failed to keep its event loop running (exit $result)" >&2
    exit 1
fi
if find "$appdir" -type f \( -name '*.ttf' -o -name '*.otf' \) | grep -q .; then
    echo "Font fixtures must not be included in release packages" >&2
    exit 1
fi
tar -C "$stage" -czf "$output/FontMaster-$version-linux-x86_64.tar.gz" FontMaster.AppDir
(
    cd "$output"
    sha256sum "FontMaster-$version-linux-x86_64.AppImage" \
        "FontMaster-$version-linux-x86_64.tar.gz" > SHA256SUMS
    sha256sum --check SHA256SUMS
)
