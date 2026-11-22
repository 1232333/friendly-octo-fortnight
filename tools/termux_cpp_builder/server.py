#!/usr/bin/env python3
"""Local-only build agent for Godot projects edited on Android.

Run inside Termux. It intentionally binds to 127.0.0.1 and only accepts
projects stored in Android shared storage.
"""
from __future__ import annotations

import json
import hmac
import secrets
import os
import platform
import shutil
import subprocess
import sys
import time
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

HOST = "127.0.0.1"
PORT = 8766
TOKEN_PATH = Path.home() / ".plugin_factory_builder" / "token"
AUTH_TOKEN = ""
MAX_BODY = 1024 * 1024
EXCLUDED_DIRS = {
    ".git", ".godot", ".plugin_factory", "__pycache__", ".pytest_cache",
    "node_modules", "dist", "build", ".gradle"
}
EXCLUDED_SUFFIXES = {".tmp", ".pyc", ".pyo"}


def shared_roots() -> list[Path]:
    roots = [Path("/storage/emulated/0"), Path("/sdcard"), Path("/storage/self/primary")]
    shared = Path.home() / "storage" / "shared"
    try:
        roots.insert(0, shared.resolve(strict=True))
    except OSError:
        pass
    result: list[Path] = []
    for root in roots:
        try:
            resolved = root.resolve(strict=True)
            if resolved not in result:
                result.append(resolved)
        except OSError:
            continue
    return result


def resolve_project(raw: str) -> Path:
    if not raw or len(raw) > 4096:
        raise ValueError("project_dir 缺失或路径过长")
    project = Path(raw).expanduser().resolve(strict=True)
    if not project.is_dir() or not (project / "project.godot").is_file():
        raise ValueError("所选目录不是 Godot 项目（没有 project.godot）")
    roots = shared_roots()
    if not any(project == root or root in project.parents for root in roots):
        raise ValueError(
            "出于安全限制，只允许访问 Android 共享存储中的项目。"
            "请先在 Termux 执行 termux-setup-storage。"
        )
    return project


def run_command(command: list[str], cwd: Path, log_file: Path, timeout: int = 3600) -> dict:
    log_file.parent.mkdir(parents=True, exist_ok=True)
    started = time.time()
    with log_file.open("w", encoding="utf-8") as log:
        log.write("$ " + " ".join(command) + "\n\n")
        log.flush()
        try:
            completed = subprocess.run(
                command, cwd=str(cwd), stdout=log, stderr=subprocess.STDOUT,
                text=True, errors="replace", timeout=timeout, check=False
            )
            code = completed.returncode
        except subprocess.TimeoutExpired:
            log.write("\nERROR: command timed out after %d seconds\n" % timeout)
            return {"ok": False, "exit_code": -1, "error": "构建超时", "log": str(log_file)}
        except OSError as error:
            log.write("\nERROR: %s\n" % error)
            return {"ok": False, "exit_code": -1, "error": str(error), "log": str(log_file)}
    return {
        "ok": code == 0,
        "exit_code": code,
        "elapsed_seconds": round(time.time() - started, 2),
        "log": str(log_file),
    }


def tool_version(command: list[str]) -> str | None:
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        output = (result.stdout or result.stderr).strip().splitlines()
        return output[0] if output else None
    except (OSError, subprocess.SubprocessError):
        return None


def health() -> dict:
    versions = {
        "python": platform.python_version(),
        "architecture": platform.machine(),
        "clang": tool_version(["clang++", "--version"]),
        "cmake": tool_version(["cmake", "--version"]),
        "ninja": tool_version(["ninja", "--version"]),
        "git": tool_version(["git", "--version"]),
    }
    target = tool_version(["clang++", "-dumpmachine"]) if versions["clang"] else None
    required = all(versions[name] for name in ("clang", "cmake", "ninja", "git"))
    ndk_root = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT", "")
    toolchain = Path(ndk_root) / "build" / "cmake" / "android.toolchain.cmake" if ndk_root else None
    ndk_ready = bool(toolchain and toolchain.is_file())
    android_target = bool(target and "android" in target.lower())
    return {
        "ok": required and platform.machine() in ("aarch64", "arm64") and android_target and ndk_ready,
        "service": "Plugin Factory Termux C++ Builder",
        "version": "0.1.0",
        "port": PORT,
        "versions": versions,
        "clang_target": target,
        "android_ndk_home": str(ndk_root) if ndk_root else "",
        "ndk_toolchain_ready": ndk_ready,
        "message": (
            "Termux Android ARM64 编译环境就绪"
            if required and android_target and ndk_ready
            else "请安装 Termux clang、CMake、Ninja、Git，并完成 Android NDK 安装（ANDROID_NDK_HOME）"
        ),
    }


def build_cpp(project: Path, payload: dict) -> dict:
    source = project / "cpp"
    cmake_file = source / "CMakeLists.txt"
    addon = project / "addons" / "chinese_programming_language"
    if not cmake_file.is_file():
        raise ValueError("项目缺少 cpp/CMakeLists.txt。请先在插件工厂点击“创建中文编程插件”。")
    if not (addon / "plugin.cfg").is_file() or not (addon / "chinese_native.gdextension").is_file():
        raise ValueError("缺少 addons/chinese_programming_language 插件封装，请重新点击“创建中文编程插件”。")

    compiler_target = tool_version(["clang++", "-dumpmachine"]) or ""
    if "android" not in compiler_target.lower():
        raise ValueError(
            "当前 clang++ 不是 Android 目标编译器："
            + compiler_target
            + "。请使用原生 Termux 的 clang，不要使用 Linux 交叉环境。"
        )
    if platform.machine() not in ("aarch64", "arm64"):
        raise ValueError("当前设备不是 ARM64，不能生成 Android ARM64 扩展。")

    ndk_root = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT", "")
    toolchain = Path(ndk_root) / "build" / "cmake" / "android.toolchain.cmake" if ndk_root else None
    if not toolchain or not toolchain.is_file():
        raise ValueError("找不到 Android NDK CMake 工具链。请重新运行 Termux 安装脚本。")

    cmake = shutil.which("cmake")
    ninja = shutil.which("ninja")
    if not cmake or not ninja:
        raise ValueError("缺少 CMake 或 Ninja，请运行 Termux 安装脚本。")

    build_dir = project / ".plugin_factory" / "cpp-build"
    log_file = project / ".plugin_factory" / "cpp-build.log"
    jobs = max(1, min(int(payload.get("jobs", 2)), 4))
    configure = [
        cmake, "-S", str(source), "-B", str(build_dir), "-G", "Ninja",
        "--toolchain", str(toolchain),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DANDROID_ABI=arm64-v8a",
        "-DANDROID_PLATFORM=android-24",
        "-DANDROID_STL=c++_static",
        "-DPLUGIN_FACTORY_ANDROID_TERMUX=ON",
    ]
    configured = run_command(configure, project, log_file, timeout=1800)
    if not configured["ok"]:
        return {"ok": False, "stage": "configure", **configured}

    built = run_command(
        [cmake, "--build", str(build_dir), "--parallel", str(jobs)],
        project, log_file, timeout=3600
    )
    if not built["ok"]:
        return {"ok": False, "stage": "compile", **built}

    candidates = list(build_dir.rglob("libchinese_native.so"))
    if not candidates:
        return {
            "ok": False, "stage": "collect",
            "error": "编译命令成功，但没有找到 libchinese_native.so",
            "log": str(log_file),
        }
    output = project / "addons" / "chinese_programming_language" / "bin" / "android"
    output.mkdir(parents=True, exist_ok=True)
    target = output / "libchinese_native.so"
    shutil.copy2(candidates[0], target)
    return {
        "ok": True, "stage": "complete", "library": str(target),
        "size_bytes": target.stat().st_size, "log": str(log_file),
        "message": "中文编程语言 Android ARM64 GDExtension 已安装到 Godot 插件目录",
    }


def package_chinese_plugin(project: Path) -> dict:
    addon = project / "addons" / "chinese_programming_language"
    descriptor = addon / "chinese_native.gdextension"
    plugin_cfg = addon / "plugin.cfg"
    library = addon / "bin" / "android" / "libchinese_native.so"
    for required in (plugin_cfg, descriptor, addon / "plugin.gd", library):
        if not required.is_file():
            raise ValueError("中文编程插件尚未准备完整，缺少文件：" + str(required.relative_to(project)))

    dist = project / "dist"
    dist.mkdir(exist_ok=True)
    output = dist / "ChineseProgrammingLanguage-Android-ARM64.zip"
    temp = output.with_suffix(".zip.tmp")
    file_count = 0
    with zipfile.ZipFile(temp, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for current, dirs, files in os.walk(addon):
            dirs[:] = sorted(d for d in dirs if d not in EXCLUDED_DIRS)
            for filename in sorted(files):
                path = Path(current) / filename
                relative = path.relative_to(project)
                archive.write(path, relative.as_posix())
                file_count += 1
    temp.replace(output)
    return {
        "ok": True, "package": str(output), "size_bytes": output.stat().st_size,
        "file_count": file_count,
        "message": "可安装到 Godot 的中文编程语言 Android ARM64 插件 ZIP 已生成",
    }


def package_project(project: Path) -> dict:
    dist = project / "dist"
    dist.mkdir(exist_ok=True)
    output = dist / (project.name + "-Android-ARM64-Project.zip")
    temp = output.with_suffix(".zip.tmp")
    file_count = 0
    with zipfile.ZipFile(temp, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for current, dirs, files in os.walk(project):
            current_path = Path(current)
            dirs[:] = sorted(
                d for d in dirs
                if d not in EXCLUDED_DIRS and not d.startswith(".Trash")
            )
            for filename in sorted(files):
                path = current_path / filename
                if path.suffix.lower() in EXCLUDED_SUFFIXES:
                    continue
                if path == output or path == temp:
                    continue
                relative = path.relative_to(project)
                archive.write(path, relative.as_posix())
                file_count += 1
    temp.replace(output)
    return {
        "ok": True, "package": str(output),
        "size_bytes": output.stat().st_size, "file_count": file_count,
        "message": "Godot 项目 ZIP 打包完成（不包含编辑器缓存和中间构建目录）",
    }


class Handler(BaseHTTPRequestHandler):
    server_version = "PluginFactoryBuilder/0.1"

    def _send(self, status: int, data: dict) -> None:
        body = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        if urlparse(self.path).path != "/health":
            self._send(404, {"ok": False, "error": "接口不存在"})
            return
        self._send(200, health())

    def do_POST(self) -> None:
        if not AUTH_TOKEN or not hmac.compare_digest(
            self.headers.get("X-Plugin-Factory-Token", ""), AUTH_TOKEN
        ):
            self._send(403, {"ok": False, "error": "连接密钥无效，请复制 Termux 启动时显示的密钥。"})
            return
        route = urlparse(self.path).path
        if route not in ("/build", "/package", "/package_plugin"):
            self._send(404, {"ok": False, "error": "接口不存在"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0 or length > MAX_BODY:
                raise ValueError("请求数据为空或超过大小限制")
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            project = resolve_project(str(payload.get("project_dir", "")))
            if route == "/build":
                result = build_cpp(project, payload)
            elif route == "/package_plugin":
                result = package_chinese_plugin(project)
            else:
                result = package_project(project)
            self._send(200 if result.get("ok") else 500, result)
        except (ValueError, OSError, json.JSONDecodeError) as error:
            self._send(400, {"ok": False, "error": str(error)})

    def log_message(self, fmt: str, *args) -> None:
        print("[HTTP] " + (fmt % args), flush=True)


def main() -> None:
    global AUTH_TOKEN
    TOKEN_PATH.parent.mkdir(parents=True, exist_ok=True)
    if TOKEN_PATH.is_file():
        AUTH_TOKEN = TOKEN_PATH.read_text(encoding="utf-8").strip()
    if not AUTH_TOKEN:
        AUTH_TOKEN = secrets.token_urlsafe(32)
        TOKEN_PATH.write_text(AUTH_TOKEN, encoding="utf-8")
        TOKEN_PATH.chmod(0o600)
    print("Plugin Factory Termux C++ Builder 0.1.0", flush=True)
    print("Godot 连接密钥（只在本机使用，请勿分享）：", flush=True)
    print(AUTH_TOKEN, flush=True)
    print(f"Listening only on http://{HOST}:{PORT}", flush=True)
    print("Keep this Termux session alive while Godot is building.", flush=True)
    ThreadingHTTPServer((HOST, PORT), Handler).serve_forever()


if __name__ == "__main__":
    main()
