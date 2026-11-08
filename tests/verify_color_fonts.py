"""Validate new color-font paths with FontTools, independently of C++ parsers."""
import base64
from pathlib import Path
import struct
import subprocess
import sys
import zlib
from fontTools.ttLib import TTFont

cli, font_dir, build_dir = sys.argv[1:]
fonts, output = Path(font_dir), Path(build_dir)


def run(*args):
    return subprocess.run([cli, *map(str, args)], capture_output=True, text=True, check=True)


def verify_sfnt(path):
    font = TTFont(path, checkChecksums=2)
    raw = path.read_bytes()
    padded = raw + b"\0" * (-len(raw) % 4)
    assert sum(struct.unpack(f">{len(padded)//4}I", padded)) & 0xFFFFFFFF == 0xB1B0AFBA
    count, search, selector, shift = struct.unpack_from(">4H", raw, 4)
    power = 1 << (count.bit_length() - 1)
    assert search == power * 16 and selector == power.bit_length() - 1 and shift == count * 16 - search
    for tag in font.keys()[1:]:
        entry = font.reader.tables[tag]
        assert entry.offset % 4 == 0
        data = bytearray(raw[entry.offset:entry.offset + entry.length])
        assert len(data) == entry.length
        if tag == "head":
            data[8:12] = b"\0" * 4
        data += b"\0" * (-len(data) % 4)
        assert sum(struct.unpack(f">{len(data)//4}I", data)) & 0xFFFFFFFF == entry.checkSum, (path.name, tag)
        font[tag]  # Also require successful independent decompilation.
    return font


def verify_png(path, size):
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    assert struct.unpack_from(">2I", data, 16) == (size, size)
    cursor, compressed = 8, bytearray()
    while cursor < len(data):
        length = struct.unpack_from(">I", data, cursor)[0]
        chunk = data[cursor + 4:cursor + 8 + length]
        assert len(chunk) == length + 4
        assert zlib.crc32(chunk) == struct.unpack_from(">I", data, cursor + 8 + length)[0]
        if chunk[:4] == b"IDAT":
            compressed += chunk[4:]
        cursor += length + 12
    assert cursor == len(data) and any(zlib.decompress(compressed))


for name in ("edited-sbix.ttf", "svg-edited.ttf", "svg-empty.ttf", "colr-replaced.ttf",
             "mixed-cbdt-edited.ttf", "mixed-sbix-edited.ttf"):
    verify_sfnt(output / name)

sbix = verify_sfnt(output / "edited-sbix.ttf")["sbix"]
assert len(sbix.strikes) == 2
assert any(glyph.graphicType == "png " and glyph.imageData
           for strike in sbix.strikes.values() for glyph in strike.glyphs.values())

source = TTFont(fonts / "NotoColorEmoji.ttf")
expected_png = source["CBDT"].strikeData[0][source.getBestCmap()[0x1F601]].imageData
replaced = verify_sfnt(output / "colr-replaced.ttf")
gid = replaced.getGlyphID(replaced.getBestCmap()[0x1F600])
document = next(doc.data for doc in replaced["SVG "].docList if doc.startGlyphID <= gid <= doc.endGlyphID)
encoded = document.split("data:image/png;base64,", 1)[1].split("'", 1)[0].split('"', 1)[0]
assert base64.b64decode(encoded) == expected_png

for kind in ("ttf", "cff", "cff2"):
    font = verify_sfnt(output / f"edited-standard-{kind}.otf")
    assert font["maxp"].numGlyphs == 5
    assert set(font.getBestCmap()).isdisjoint({32, 66, 90})
    assert font.getGlyphID(font.getBestCmap()[65]) == 1
    assert font.getGlyphID(font.getBestCmap()[67]) == 4
    assert font.reader["TEST"] == b"preserve unrelated custom data"

for font in (fonts / "Noto-COLRv1.ttf", fonts / "NotoColorEmoji.ttf", output / "colr-replaced.ttf"):
    png = output / f"cli-{font.stem}.png"
    run("render", "--font", font, "--unicode", "U+1F600", "--size", 96, "--output", png)
    verify_png(png, 96)

# Deletion of an ordinary glyph must be identical before and after a saved
# PNG replacement changes dispatch from the outline handler to SVG.
mixed = output / "cli-outline-png.otf"
removed = output / "cli-outline-removed.otf"
run("replace", "--font", output / "standard-ttf.otf", "--name", "glyph_2",
    "--image", output / "cli-Noto-COLRv1.png", "--output", mixed)
run("remove", "--font", mixed, "--name", "glyph_2", "--output", removed)
checked = verify_sfnt(removed)
assert 66 not in checked.getBestCmap()
assert checked["glyf"][checked.getGlyphName(2)].numberOfContours == 0

for extra in (("--size", "0"), ("--size", "128junk"), ("--unicode", "110000")):
    command = [cli, "render", "--font", str(fonts / "Noto-COLRv1.ttf"),
               "--unicode", "1F600", "--output", str(output / "invalid-render.png"), *extra]
    result = subprocess.run(command, capture_output=True, text=True)
    assert result.returncode != 0, command

print("FontTools verified SVG/SBIX/COLR/TTF/CFF/CFF2, checksums, embedded PNGs and CLI render/edit round trips")
