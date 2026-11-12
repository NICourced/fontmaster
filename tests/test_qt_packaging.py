import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest


SOURCE = Path(__file__).resolve().parent.parent
BUILD = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else None


class QtPackagingTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="fontmaster Qt packaging ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        self.build.mkdir()

    def installation(self, major, plugins):
        root = self.root / f"Qt {major}"
        platforms = root / "plugins" / "platforms"
        platforms.mkdir(parents=True)
        for name in ["libqoffscreen.so", *plugins]:
            (platforms / name).write_text("fixture")
        qmake = root / "qmake"
        qmake.write_text(
            "#!/bin/bash\nset -eu\ncase $2 in\n"
            f"QT_VERSION) printf '%s\\n' '{major}.12.0';;\n"
            f"QT_INSTALL_PLUGINS) printf '%s\\n' {shlex.quote(str(root / 'plugins'))};;\n"
            "*) exit 1;;\nesac\n"
        )
        qmake.chmod(0o755)
        return qmake

    def configure(self, major, qmake):
        (self.build / "fontmaster-qt-tools.txt").write_text(f"{major}\n{qmake}\n")

    def settings(self, override=None):
        environment = dict(os.environ)
        environment.pop("QMAKE", None)
        if override:
            environment["QMAKE"] = str(override)
        return subprocess.run(
            ["bash", "-c", 'set -euo pipefail; source "$1" "$2"; '
             'bash -c \'printf "%s\\n%s\\n" "$QMAKE" "$EXTRA_PLATFORM_PLUGINS"\'',
             "qt-packaging-test", str(SOURCE / "ci/qt-package-config.sh"), str(self.build)],
            env=environment, text=True, capture_output=True, timeout=10,
        )

    def assert_settings(self, result, qmake, plugins):
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        self.assertEqual(lines[0], str(qmake))
        self.assertEqual(set(lines[1].split(";")), {"libqoffscreen.so", *plugins})

    def test_qt5_build_uses_its_qmake_even_with_qmake6_in_path(self):
        qmake5 = self.installation(5, ["libqwayland-generic.so", "libqwayland-egl.so"])
        qmake6 = self.installation(6, ["libqwayland.so"])
        bin_dir = self.root / "bin"
        bin_dir.mkdir()
        (bin_dir / "qmake6").symlink_to(qmake6)
        self.configure(5, qmake5)
        original_path = os.environ["PATH"]
        os.environ["PATH"] = str(bin_dir) + os.pathsep + original_path
        try:
            self.assert_settings(self.settings(), qmake5,
                                 ["libqwayland-generic.so", "libqwayland-egl.so"])
        finally:
            os.environ["PATH"] = original_path

    def test_qt6_supports_unified_wayland_plugin(self):
        qmake = self.installation(6, ["libqwayland.so"])
        self.configure(6, qmake)
        self.assert_settings(self.settings(), qmake, ["libqwayland.so"])

    def test_qt6_supports_older_wayland_plugin_names(self):
        plugins = ["libqwayland-generic.so", "libqwayland-egl.so"]
        qmake = self.installation(6, plugins)
        self.configure(6, qmake)
        self.assert_settings(self.settings(), qmake, plugins)

    def test_incompatible_qmake_override_is_rejected(self):
        qmake5 = self.installation(5, ["libqwayland-generic.so"])
        qmake6 = self.installation(6, ["libqwayland.so"])
        self.configure(5, qmake5)
        result = self.settings(override=qmake6)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Qt 5", result.stderr)

    def test_missing_wayland_plugins_are_rejected(self):
        qmake = self.installation(6, [])
        self.configure(6, qmake)
        result = self.settings()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Wayland", result.stderr)

    @unittest.skipUnless(BUILD, "No configured build directory provided")
    def test_cmake_records_qmake_from_the_selected_qt_installation(self):
        tools = (BUILD / "fontmaster-qt-tools.txt").read_text().splitlines()
        self.assertEqual(len(tools), 2)
        major, qmake = tools
        version = subprocess.check_output([qmake, "-query", "QT_VERSION"], text=True)
        self.assertEqual(version.strip().split(".")[0], major)
        dependencies = subprocess.check_output(["ldd", str(BUILD / "fontmaster_qt")], text=True)
        self.assertIn(f"libQt{major}Widgets.so.{major}", dependencies)


if __name__ == "__main__":
    unittest.main()
