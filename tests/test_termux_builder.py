import importlib.util
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

MODULE_PATH = Path(__file__).parents[1] / "tools" / "termux_cpp_builder" / "server.py"
SPEC = importlib.util.spec_from_file_location("plugin_factory_builder", MODULE_PATH)
builder = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(builder)


class BuilderTests(unittest.TestCase):
    def test_project_must_contain_project_godot_and_be_in_shared_storage(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            project = root / "MyGame"
            project.mkdir()
            (project / "project.godot").write_text("config_version=5\n", encoding="utf-8")
            with patch.object(builder, "shared_roots", return_value=[root]):
                self.assertEqual(builder.resolve_project(str(project)), project.resolve())

    def test_project_outside_shared_storage_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary) / "MyGame"
            project.mkdir()
            (project / "project.godot").write_text("config_version=5\n", encoding="utf-8")
            with patch.object(builder, "shared_roots", return_value=[]):
                with self.assertRaises(ValueError):
                    builder.resolve_project(str(project))

    def test_package_chinese_plugin_requires_android_binary_and_packages_addon_root(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary) / "MyGame"
            addon = project / "addons" / "chinese_programming_language"
            binary = addon / "bin" / "android" / "libchinese_native.so"
            binary.parent.mkdir(parents=True)
            project.mkdir(exist_ok=True)
            (project / "project.godot").write_text("config_version=5\n", encoding="utf-8")
            (addon / "plugin.cfg").write_text('[plugin]\nscript="plugin.gd"\n', encoding="utf-8")
            (addon / "plugin.gd").write_text("@tool\nextends EditorPlugin\n", encoding="utf-8")
            (addon / "chinese_native.gdextension").write_text('[configuration]\n', encoding="utf-8")
            binary.write_bytes(b"android-arm64-library")
            result = builder.package_chinese_plugin(project)
            self.assertTrue(result["ok"])
            with zipfile.ZipFile(result["package"]) as archive:
                names = set(archive.namelist())
            self.assertIn("addons/chinese_programming_language/plugin.cfg", names)
            self.assertIn("addons/chinese_programming_language/plugin.gd", names)
            self.assertIn("addons/chinese_programming_language/chinese_native.gdextension", names)
            self.assertIn("addons/chinese_programming_language/bin/android/libchinese_native.so", names)

    def test_package_chinese_plugin_fails_without_android_binary(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary) / "MyGame"
            addon = project / "addons" / "chinese_programming_language"
            addon.mkdir(parents=True)
            (addon / "plugin.cfg").write_text("[plugin]\n", encoding="utf-8")
            (addon / "plugin.gd").write_text("@tool\n", encoding="utf-8")
            (addon / "chinese_native.gdextension").write_text("[configuration]\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                builder.package_chinese_plugin(project)

    def test_package_excludes_editor_cache_and_git(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary) / "MyGame"
            project.mkdir()
            (project / "project.godot").write_text("config_version=5\n", encoding="utf-8")
            (project / "main.gd").write_text("extends Node\n", encoding="utf-8")
            (project / ".godot").mkdir()
            (project / ".godot" / "cache.bin").write_bytes(b"cache")
            (project / ".git").mkdir()
            (project / ".git" / "config").write_text("secret", encoding="utf-8")
            (project / "bin" / "native").mkdir(parents=True)
            (project / "bin" / "native" / "libchinese_native.so").write_bytes(b"binary")
            result = builder.package_project(project)
            self.assertTrue(result["ok"])
            with zipfile.ZipFile(result["package"]) as archive:
                names = set(archive.namelist())
            self.assertIn("project.godot", names)
            self.assertIn("main.gd", names)
            self.assertIn("bin/native/libchinese_native.so", names)
            self.assertFalse(any(name.startswith(".godot/") for name in names))
            self.assertFalse(any(name.startswith(".git/") for name in names))


if __name__ == "__main__":
    unittest.main()
