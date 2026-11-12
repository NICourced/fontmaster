#!/usr/bin/env bash

mapfile -t qt_build_tools < "${1:?Build directory required}/fontmaster-qt-tools.txt"
if [[ ${#qt_build_tools[@]} != 2 || ! ${qt_build_tools[0]} =~ ^[56]$ ]]; then
    echo "Invalid Qt build metadata; reconfigure the build directory" >&2
    return 1
fi
qt_major=${qt_build_tools[0]}
qmake_bin=${QMAKE:-${qt_build_tools[1]}}
if ! qmake_bin=$(command -v "$qmake_bin"); then
    echo "Cannot locate qmake for the Qt $qt_major build" >&2
    return 1
fi
qt_version=$("$qmake_bin" -query QT_VERSION)
if [[ ${qt_version%%.*} != "$qt_major" ]]; then
    echo "Qt $qt_major build cannot be packaged with qmake $qt_version" >&2
    return 1
fi
export QMAKE="$qmake_bin"
qt_plugins=$("$QMAKE" -query QT_INSTALL_PLUGINS)
qt_platform_plugins=(libqoffscreen.so)
for platform in libqwayland.so libqwayland-generic.so libqwayland-egl.so; do
    if [[ -s "$qt_plugins/platforms/$platform" ]]; then
        qt_platform_plugins+=("$platform")
    fi
done
if [[ ${#qt_platform_plugins[@]} == 1 ]]; then
    echo "No Qt $qt_major Wayland platform plugin found in $qt_plugins" >&2
    return 1
fi
if [[ ! -s "$qt_plugins/platforms/libqoffscreen.so" ]]; then
    echo "Qt $qt_major offscreen platform plugin is required for package verification" >&2
    return 1
fi
EXTRA_PLATFORM_PLUGINS=$(IFS=';'; printf '%s' "${qt_platform_plugins[*]}")
export EXTRA_PLATFORM_PLUGINS
