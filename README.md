# FontMaster

Inspect, render and edit compiled OpenType fonts from C++ or the command line, with an optional Qt desktop interface.

FontMaster works directly with binary `.ttf` and `.otf` files, including color emoji fonts. Use it to explore glyphs, export PNG previews, replace glyph artwork and save modified fonts.

[Download](#download) · [Features](#features) · [Build](#build) · [Install](#install) · [Command line](#command-line) · [Desktop app](#desktop-app) · [C++ API](#c-api) · [License](#license)

## Download

Download Linux x86_64 binaries from [GitHub Releases](https://github.com/NICourced/fontmaster/releases/latest). Packages are built on Ubuntu 24.04 and require glibc 2.39 or newer.

- **AppImage:** the Qt desktop app with bundled application dependencies. Make it executable with `chmod +x FontMaster-*.AppImage` and run it. If FUSE is unavailable, run it with `--appimage-extract-and-run`.
- **Portable archive:** the Qt app, CLI and bundled libraries. Extract the `.tar.gz`, launch `FontMaster.AppDir/AppRun` for the desktop app, or run `FontMaster.AppDir/usr/bin/fontmaster_cli --help` for the CLI.
- **SHA256SUMS:** verify downloads with `sha256sum --check SHA256SUMS` from the directory containing both release files.

The packages use the host's graphics drivers and desktop services. Test fonts are excluded. GitHub Actions builds and tests pushes and pull requests; pushing a version tag matching the CMake version publishes the packages to a release after the build succeeds.

## Features

- List glyph names, Unicode mappings and image metadata.
- Render glyphs to transparent PNG images at sizes from 1 to 4096 pixels.
- Replace glyph artwork with PNG or SVG, depending on the font format.
- Remove glyphs or color presentations, with checks for dependent glyphs.
- Preview edits in memory before saving the font.
- Use the same library through a CLI, a Qt interface or your own C++ application.

### Supported formats

| Format | Capabilities |
| --- | --- |
| **CBDT/CBLC** | Read bitmap strikes, render and extract PNG artwork, replace PNG images and remove color records. Supports CBLC index formats 1–5 and CBDT PNG formats 17–19. |
| **SBIX** | Read PNG, JPEG and TIFF artwork across strikes, resolve `dupe` references, replace with PNG and remove color records. |
| **SVG** | Render plain or gzip-compressed SVG documents, replace artwork with PNG or SVG, and edit individual glyphs in shared document ranges. |
| **COLR/CPAL v0 and v1** | Render layered color glyphs, gradients, transforms and compositing; remove color presentations or add PNG/SVG replacements. |
| **TrueType, CFF and CFF2 outlines** | List and render ordinary glyphs, remove outlines with HarfBuzz Subset, and add PNG/SVG replacements. |
| **Mixed SVG and bitmap fonts** | Combine glyph listings and render SVG presentations with priority over bitmap artwork. |

## Build

### Requirements

You need a C++17 compiler, CMake and `pkg-config`, plus the following development libraries:

| Dependency | Minimum version / pkg-config module |
| --- | --- |
| HarfBuzz | 15.1 / `harfbuzz` |
| HarfBuzz Raster | 15.1 / `harfbuzz-raster` |
| HarfBuzz Subset | 11.1 / `harfbuzz-subset` |
| Cairo | `cairo` |
| librsvg | 2.46 / `librsvg-2.0` |
| libpng | `libpng` |
| libjpeg-turbo | 3.0 / `libturbojpeg` |
| libtiff | 4.5 / `libtiff-4` |
| libxml2 | `libxml-2.0` |
| zlib | `zlib` |

**Optional:** Qt5 Core and Widgets build the desktop app. Python 3 with FontTools enables additional output-validation tests; it is not required at runtime.

### Compile

```sh
git clone https://github.com/NICourced/fontmaster.git
cd fontmaster
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The build produces:

| Target | Description |
| --- | --- |
| `fontmaster` | Shared C++ library |
| `fontmaster_cli` | Command-line application |
| `fontmaster_qt` | Desktop application, built when Qt5 is available |

To explicitly build without the desktop app, add `-DCMAKE_DISABLE_FIND_PACKAGE_Qt5=TRUE` to the configure command. To disable tests, add `-DBUILD_TESTING=OFF`.

## Install

Installation is an explicit step after building. Choose either a system-wide or a user-local installation; building alone does not install anything.

### System-wide

Install under `/usr/local` for all users:

```sh
sudo cmake --install build --prefix /usr/local
```

### User-local

Install under your home directory without administrator privileges:

```sh
cmake --install build --prefix "$HOME/.local"
```

Add the executables to the current shell's search path, or put this line in your shell configuration:

```sh
export PATH="$HOME/.local/bin:$PATH"
```

You can then run `fontmaster_cli --help` or `fontmaster_qt` if the desktop app was built. On Linux, the installed applications locate FontMaster's shared library relative to their location, so a user-local installation does not require `ldconfig` or `LD_LIBRARY_PATH`. Third-party runtime dependencies must still be available.

Both modes install the library, public headers, CLI, optional Qt app, README and CMake package files. Directory names follow [GNUInstallDirs](https://cmake.org/cmake/help/latest/module/GNUInstallDirs.html); configure with options such as `-DCMAKE_INSTALL_LIBDIR=lib64` to override them. You can also set the default prefix during configuration with `-DCMAKE_INSTALL_PREFIX=/your/prefix` and then run `cmake --install build`.

For packaging or a trial installation, stage a system layout in a temporary directory:

```sh
DESTDIR="$(mktemp -d /tmp/fontmaster-stage.XXXXXX)" \
  cmake --install build --prefix /usr/local
```

This writes beneath the staging directory, leaving `/usr/local` unchanged. The `cmake --install` commands require CMake 3.15 or newer; with older CMake, set `CMAKE_INSTALL_PREFIX` during configuration and use `cmake --build build --target install`.

## Command line

Run the following examples from the repository root after placing your own copies of the named Noto Emoji fonts in a local `fonts/` directory. Font files are not tracked in this repository.

### Inspect a font

```sh
./build/fontmaster_cli info fonts/NotoColorEmoji.ttf
./build/fontmaster_cli list fonts/Noto-COLRv1.ttf
```

Use `list` to find a glyph's name before replacing its artwork. Names and glyph IDs vary between fonts.

### Export a PNG preview

```sh
./build/fontmaster_cli render \
  --font fonts/Noto-COLRv1.ttf \
  --unicode 1F600 \
  --size 256 \
  --output grinning.png
```

`--unicode` accepts a hexadecimal code point, such as `1F600` or `U+1F600`. You can use `--name` instead to select a glyph by name. The default size is 128 pixels; the output is a square PNG with a transparent background. `export` is an alias for `render`.

### Remove a glyph

```sh
./build/fontmaster_cli remove \
  --font fonts/NotoColorEmoji.ttf \
  --unicode 1F600 \
  --output modified.ttf
```

For color fonts, removal clears the selected color presentation while preserving unrelated outlines and mappings. For ordinary outline fonts, it removes the outline and its Unicode mappings. Dependencies such as referenced outline components are checked before editing.

### Replace artwork

Replace `GLYPH_NAME` below with a name returned by `list`, and provide a PNG image:

```sh
./build/fontmaster_cli replace \
  --font fonts/NotoColorEmoji.ttf \
  --name GLYPH_NAME \
  --image replacement.png \
  --output replaced.ttf
```

CBDT/CBLC and SBIX accept PNG replacements. SVG, COLR/CPAL and outline fonts accept PNG or SVG artwork. If `--output` is omitted for `remove` or `replace`, the result is saved to `<input-path>.modified.ttf`.

For the full command syntax:

```sh
./build/fontmaster_cli --help
```

## Desktop app

When Qt5 is installed, launch:

```sh
./build/fontmaster_qt
```

Open a `.ttf` or `.otf` file to browse glyphs and preview their artwork. Select a glyph to replace its image or remove it, then save the modified font. Previews reflect the current edits before saving.

## C++ API

The public headers are in [`include/fontmaster/`](include/fontmaster/). This example loads a font, renders U+1F600 and writes a PNG:

```cpp
#include <fontmaster/FontMaster.h>
#include <fontmaster/EmojiRenderer.h>

#include <fstream>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc != 2) return 1;

    auto font = fontmaster::Font::load(argv[1]);
    const auto name = font->findGlyphName(0x1F600);
    if (name.empty()) throw std::runtime_error("Font has no U+1F600 glyph");

    const auto png = fontmaster::renderGlyph(*font, name, 256);
    std::ofstream output("grinning.png", std::ios::binary);
    output.write(reinterpret_cast<const char*>(png.data()), png.size());
    return output ? 0 : 1;
}
```

To use an installed FontMaster library, save the example as `example.cpp` and add the following to your CMake project:

```cmake
find_package(fontmaster 0.0.1 CONFIG REQUIRED)
add_executable(example example.cpp)
target_link_libraries(example PRIVATE fontmaster::fontmaster)
```

For a user-local or custom installation, pass its prefix when configuring your application, for example `cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/.local"`.

If you chose a library directory that CMake does not search on your platform, point directly to the package directory instead, for example `-Dfontmaster_DIR="$HOME/.local/lib64/cmake/fontmaster"`.

Alternatively, embed the repository at `external/fontmaster`:

```cmake
add_subdirectory(external/fontmaster)
add_executable(example example.cpp)
target_link_libraries(example PRIVATE fontmaster::fontmaster)
```

Run the resulting example with a font path as its argument. `Font` also exposes `listGlyphMetadata()`, `listGlyphs()`, `replaceGlyphImage()`, `removeGlyph()` and `save()`. Use `listGlyphMetadata()` for names, formats and sizes without copying image payloads; fetch artwork for an individual glyph with `getGlyphInfo()`. `listGlyphs()` includes image payloads and can consume substantial memory when many glyphs share one SVG document. The free `renderGlyph()` function uses the current in-memory font, including unsaved edits.

## Editing behavior and limits

- **COLR and outline replacements use SVG.** Replacing artwork adds an SVG presentation. Unrelated vector data remains intact; shared COLR definitions needed by other glyphs are retained.
- **Rendering in other applications may differ.** FontMaster prefers SVG when multiple presentations exist. Other applications may choose a different color table; PNG export captures FontMaster's rendering.
- **Bitmap replacement updates existing strikes.** Each strike containing the glyph receives the supplied PNG. Artwork is not automatically resized for each strike, and existing bearings and advances are preserved.
- **CBDT replacement PNGs are limited to 255 × 255 pixels.** Other image paths accept dimensions up to 8192 pixels, subject to input and allocation limits.
- **Outline removal preserves remaining glyph IDs.** An unused final glyph slot may be trimmed. Removing `.notdef` or an outline component still used by another glyph is rejected.

## Contributing

Bug reports should include the command or steps to reproduce, the font format, the expected result and the actual output. Include a sample font or a source link when its license permits sharing.

For code changes, build the project and run CTest before submitting a pull request. Changes to binary parsing or writing should include a regression test that checks the saved font after reloading it.

## License

FontMaster is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version (**GPL-3.0-or-later**). See [LICENSE](LICENSE) for the full license text.

FontMaster is distributed without any warranty, including the implied warranties of merchantability or fitness for a particular purpose. Third-party dependencies and fonts retain their own licenses.
