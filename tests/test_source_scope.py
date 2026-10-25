"""Keep the vendored Vulkan fork limited to the effects we actually ship."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
VKBASALT = ROOT / "third_party" / "vkbasalt"


class SourceScopeTests(unittest.TestCase):
    def test_all_vendored_cpp_files_are_accounted_for(self):
        meson = (VKBASALT / "src" / "meson.build").read_text(encoding="utf-8")
        listed = set(re.findall(r"'([^']+\.cpp)'", meson))
        actual = {path.name for path in (VKBASALT / "src").glob("*.cpp")}
        self.assertEqual(actual, listed)

    def test_only_lut_shaders_remain(self):
        shader_dir = VKBASALT / "src" / "shader"
        self.assertEqual(
            {path.name for path in shader_dir.iterdir()},
            {"meson.build", "full_screen_triangle.vert.glsl", "lut.frag.glsl"},
        )
        self.assertFalse((VKBASALT / "src" / "reshade").exists())
        self.assertFalse((ROOT / "third_party" / "spirv-headers").exists())

    def test_required_notices_remain(self):
        package_script = (ROOT / "contrib" / "package.sh").read_text(encoding="utf-8")
        for name in ("THIRD_PARTY_LICENSES.md", "THIRD_PARTY_LICENSES.ja.md"):
            with self.subTest(name=name):
                self.assertTrue((ROOT / name).is_file())
                self.assertIn(name, package_script)
        for name in (
            "frameeyeosc-LICENSE",
            "openvr/LICENSE",
            "vkbasalt/LICENSE",
        ):
            with self.subTest(name=name):
                self.assertTrue((ROOT / "third_party" / name).is_file())

    def test_release_extracts_to_stable_directory(self):
        package_script = (ROOT / "contrib" / "package.sh").read_text()
        self.assertIn('fc_stage_dir="$fc_temp_dir/framecolor"', package_script)
        self.assertIn('tar -czf "$fc_archive" -C "$fc_temp_dir" framecolor', package_script)

    def test_no_gamescope_runtime(self):
        self.assertFalse((ROOT / "src" / "gamescope_look.cpp").exists())
        self.assertFalse((ROOT / "reshade" / "framecolor.fx").exists())
        self.assertFalse((ROOT / "third_party" / "gamescope-protocol-LICENSE").exists())
        for name in ("CMakeLists.txt", "contrib/package.sh", "contrib/framecolor.service"):
            with self.subTest(name=name):
                self.assertNotIn("gamescope", (ROOT / name).read_text(encoding="utf-8").lower())

    def test_release_source_excludes_capture(self):
        self.assertFalse((VKBASALT / "src" / "vulkan_capture.cpp").exists())
        self.assertFalse((VKBASALT / "src" / "vulkan_capture.hpp").exists())
        for name in ("src/framecolor_main.cpp", "contrib/framecolorctl", "contrib/package.sh"):
            with self.subTest(name=name):
                self.assertNotIn("capture", (ROOT / name).read_text(encoding="utf-8").lower())


if __name__ == "__main__":
    unittest.main()
