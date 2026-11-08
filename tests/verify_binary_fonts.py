"""Independent checks of the files produced by the C++ regression tests."""
from pathlib import Path
import struct
import subprocess
import sys
import zlib

from fontTools.ttLib import TTFont
from fontTools.ttLib.tables.DefaultTable import DefaultTable

cli, font_dir, output_dir = sys.argv[1:]
fonts, output = Path(font_dir), Path(output_dir)


def verify_sfnt(path):
    data = path.read_bytes()
    padded = data + b"\0" * (-len(data) % 4)
    assert sum(struct.unpack(f">{len(padded) // 4}I", padded)) & 0xFFFFFFFF == 0xB1B0AFBA
    font = TTFont(path, checkChecksums=2)
    for tag in font.keys()[1:]:
        font.getTableData(tag)
        record = font.reader.tables[tag]
        assert record.offset % 4 == 0
        table = bytearray(data[record.offset:record.offset + record.length])
        assert len(table) == record.length
        if tag == "head":
            table[8:12] = b"\0" * 4
        table += b"\0" * (-len(table) % 4)
        assert sum(struct.unpack(f">{len(table) // 4}I", table)) & 0xFFFFFFFF == record.checkSum, tag
    return font


original = TTFont(fonts / "NotoColorEmoji.ttf")
removed = original.getBestCmap()[0x1F600]
source = original.getBestCmap()[0x1F601]
target = original.getBestCmap()[0x1F602]
for filename in ("edited.ttf", "edited-again.ttf"):
    edited = verify_sfnt(output / filename)
    assert edited.getGlyphOrder() == original.getGlyphOrder()
    assert len(edited["CBDT"].strikeData) == len(original["CBDT"].strikeData)
    for old, new in zip(original["CBDT"].strikeData, edited["CBDT"].strikeData):
        assert set(new) == set(old) - {removed}
        assert new[target].imageData == old[source].imageData
        for name in new:
            png = new[name].imageData
            cursor = 8
            while cursor < len(png):
                length = struct.unpack_from(">I", png, cursor)[0]
                assert zlib.crc32(png[cursor + 4:cursor + 8 + length]) == struct.unpack_from(">I", png, cursor + 8 + length)[0]
                cursor += length + 12
            assert cursor == len(png)
            if name != target:
                assert new[name].compile(original) == old[name].compile(original), name
    for tag in original.keys()[1:]:
        if tag not in ("CBDT", "CBLC", "head"):
            assert edited.reader[tag] == original.reader[tag], tag

old_colr = TTFont(fonts / "Noto-COLRv1.ttf")
new_colr = verify_sfnt(output / "edited-colr.ttf")
removed_colr = old_colr.getBestCmap()[0x1F600]
records = new_colr["COLR"].table.BaseGlyphList.BaseGlyphPaintRecord
assert removed_colr not in {record.BaseGlyph for record in records}
assert len(records) == old_colr["COLR"].table.BaseGlyphList.BaseGlyphCount - 1
for tag in old_colr.keys()[1:]:
    if tag not in ("COLR", "head"):
        assert new_colr.reader[tag] == old_colr.reader[tag], tag

# Exercise all five bitmap index formats, sparse glyph IDs, two strikes and
# PNG formats 17/18/19. FontTools reads the output independently of our parser.
fixture = TTFont(fonts / "NotoColorEmoji-flagsonly.ttf")
image = next(iter(fixture["CBDT"].strikeData[0].values()))
png = image.imageData
m = image.metrics
small = struct.pack(">BBbbB", m.height, m.width, m.BearingX, m.BearingY, m.Advance)
big = small + b"\0\0\0"
assert len(small) == 5
for index_format in range(1, 6):
    gids = [1, 3] if index_format in (4, 5) else [1, 2]
    image_format = 19 if index_format in (2, 5) else 18 if index_format == 3 else 17
    record = (b"" if image_format == 19 else big if image_format == 18 else small)
    record += struct.pack(">I", len(png)) + png
    cbdt = struct.pack(">I", 0x30000) + record * 2
    sub = struct.pack(">HHI", index_format, image_format, 4)
    if index_format in (1, 3):
        sub += struct.pack(">3I" if index_format == 1 else ">3H", 0, len(record), 2 * len(record))
    elif index_format in (2, 5):
        sub += struct.pack(">I", len(record)) + big
        if index_format == 5:
            sub += struct.pack(">I2H", 2, *gids)
    else:
        sub += struct.pack(">I6H", 2, gids[0], 0, gids[1], len(record), 0, 2 * len(record))
    array = struct.pack(">HHI", gids[0], gids[-1], 8) + sub
    # BitmapSize records precede the subtable lists and contain 24 bytes of line metrics.
    size = struct.pack(">4I", 104, len(array), 1, 0) + b"\0" * 24
    size += struct.pack(">HH4B", gids[0], gids[-1], 109, 109, 32, 1)
    second = bytearray(size)
    second[:4] = struct.pack(">I", 104 + len(array))
    cblc = struct.pack(">2I", 0x30000, 2) + size + second + array * 2
    for tag, data in (("CBLC", cblc), ("CBDT", cbdt)):
        raw = DefaultTable(tag)
        raw.data = data
        fixture[tag] = raw
    path = output / f"index-{index_format}.ttf"
    fixture.save(path)
    result = subprocess.run([cli, "info", str(path)], capture_output=True, text=True, check=True)
    assert "Glyph count: 2" in result.stdout, result.stdout
    saved = output / f"index-{index_format}-edited.ttf"
    subprocess.run([cli, "remove", "--font", str(path), "--name", "glyph_1", "--output", str(saved)],
                   capture_output=True, text=True, check=True)
    checked = verify_sfnt(saved)
    for strike in checked["CBDT"].strikeData:
        assert len(strike) == 1
        assert next(iter(strike.values())).imageData == png

print("FontTools verified edits, preserved tables, all checksums, five index formats and multiple strikes")
