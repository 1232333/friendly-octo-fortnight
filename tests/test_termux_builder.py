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


    def test_build_uses_termux_clang_and_ndk_sysroot_not_ndk_host_binary(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary) / "MyGame"
            source = project / "cpp"
            addon = project / "addons" / "chinese_programming_language"
            sysroot = Path(temporary) / "ndk" / "toolchains" / "llvm" / "prebuilt" / "linux-aarch64" / "sysroot"
            (source).mkdir(parents=True)
            addon.mkdir(parents=True)
            (sysroot / "usr" / "include").mkdir(parents=True)
            (sysroot / "usr" / "include" / "stdint.h").write_text("/* test */", encoding="utf-8")
            (source / "CMakeLists.txt").write_text("project(test)", encoding="utf-8")
            (addon / "plugin.cfg").write_text("[plugin]", encoding="utf-8")
            (addon / "chinese_native.gdextension").write_text("[configuration]", encoding="utf-8")
            termux_lib = Path(temporary) / "termux" / "lib"
            termux_lib.mkdir(parents=True)
            (termux_lib / "libunwind.a").write_bytes(b"fake-unwind")
            with (
                patch.dict(builder.os.environ, {"PREFIX": str(termux_lib.parent)}, clear=False),
                patch.object(builder, "tool_version", return_value="aarch64-unknown-linux-android"),
                patch.object(builder.platform, "machine", return_value="aarch64"),
                patch.object(builder.shutil, "which", side_effect=lambda name: f"/termux/bin/{name}"),
                patch.dict(builder.os.environ, {"ANDROID_NDK_HOME": str(sysroot.parents[4])}, clear=False),
            ):
                commands = []
                def fake_run(command, cwd, log_file, timeout=3600):
                    commands.append(command)
                    if len(commands) == 2:
                        output = project / ".plugin_factory" / "cpp-build" / "output"
                        output.mkdir(parents=True, exist_ok=True)
                        (output / "libchinese_native.so").write_bytes(b"fake-arm64-so")
                    return {"ok": True, "exit_code": 0, "log": str(log_file)}
                with patch.object(builder, "run_command", side_effect=fake_run):
                    result = builder.build_cpp(project, {"jobs": 2})
            self.assertTrue(result["ok"])
            configure = commands[0]
            self.assertNotIn("--toolchain", configure)
            self.assertIn("-DCMAKE_C_COMPILER=/termux/bin/clang", configure)
            self.assertIn("-DCMAKE_CXX_COMPILER=/termux/bin/clang++", configure)
            self.assertIn("-DCMAKE_CXX_COMPILER_TARGET=aarch64-linux-android24", configure)
            self.assertIn("-DCMAKE_SYSROOT=" + str(sysroot), configure)
            self.assertIn("-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -L" + str(termux_lib), configure)

    def test_allow_git_directory_registers_only_exact_checkout_path(self):
        with tempfile.TemporaryDirectory() as temporary:
            checkout = Path(temporary) / "project" / ".plugin_factory" / "cpp-build" / "_deps" / "godot-cpp-src"
            with patch.object(
                builder.subprocess, "run",
                side_effect=[
                    __import__("subprocess").CompletedProcess([], 1, stdout="", stderr=""),
                    __import__("subprocess").CompletedProcess([], 0, stdout="", stderr=""),
                ],
            ) as run:
                builder.allow_git_directory(checkout, Path(temporary) / "project")
            self.assertEqual(run.call_args_list[0].args[0], [
                "git", "config", "--global", "--get-all", "safe.directory"
            ])
            self.assertEqual(run.call_args_list[1].args[0], [
                "git", "config", "--global", "--add", "safe.directory", str(checkout.resolve())
            ])

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
