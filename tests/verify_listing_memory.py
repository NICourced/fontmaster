from functools import partial
import os
from pathlib import Path
import resource
import struct
import subprocess
import sys
import tempfile

from fontTools.ttLib import TTFont
from fontTools.ttLib.tables.DefaultTable import DefaultTable


def limit_memory(mebibytes):
    limit = mebibytes * 1024 * 1024
    resource.setrlimit(resource.RLIMIT_AS, (limit, limit))


def main():
    executable, fixtures, output, *gui = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="listing-memory-", dir=output) as directory:
        for kind, source in (
            ("svg", "NotoColorEmoji.ttf"),
            ("colr-svg", "Noto-COLRv1.ttf"),
            ("bitmap-svg", "NotoColorEmoji.ttf"),
        ):
            font = TTFont(Path(fixtures) / source)
            if kind == "svg":
                del font["CBDT"]
                del font["CBLC"]
            count = min(2048, font["maxp"].numGlyphs - 1)
            document = (
                b"<svg xmlns='http://www.w3.org/2000/svg'><g id='glyph1'>"
                b"<rect width='100' height='100'/></g><!--"
                + b"x" * (512 * 1024)
                + b"--></svg>"
            )
            raw = DefaultTable("SVG ")
            raw.data = (
                struct.pack(">HIIHHHII", 0, 10, 0, 1, 1, count, 14, len(document))
                + document
            )
            font["SVG "] = raw
            path = Path(directory) / (kind + ".ttf")
            font.save(path)
            for command in ("info", "list"):
                result = subprocess.run(
                    [executable, command, str(path)],
                    capture_output=True,
                    text=True,
                    timeout=20,
                    preexec_fn=partial(limit_memory, 384),
                )
                assert result.returncode == 0, (
                    f"{kind} {command} exceeded its memory budget or failed: "
                    f"{result.stderr}"
                )
                expected = "svg" if command == "list" else "Glyph count:"
                assert expected in result.stdout
            print(f"{kind}: info/list completed within 384 MiB address space")
            if gui:
                result = subprocess.run(
                    [gui[0], "--load-font", str(path)],
                    env=dict(os.environ, QT_QPA_PLATFORM="offscreen"),
                    capture_output=True,
                    text=True,
                    timeout=20,
                    preexec_fn=partial(limit_memory, 1024),
                )
                assert result.returncode == 0, f"{kind} GUI load failed: {result.stderr}"
                assert "rendered a preview" in result.stdout
                print(f"{kind}: GUI loaded and previewed within 1 GiB address space")


if __name__ == "__main__":
    main()
