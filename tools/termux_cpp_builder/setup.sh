#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INSTALL_DIR="$HOME/.plugin_factory_builder"
NDK_PARENT="$HOME/opt/plugin-factory-ndk"
NDK_ARCHIVE="$HOME/opt/plugin-factory-android-ndk-r29-aarch64.tar.xz"

echo "== Plugin Factory Termux C++ Builder setup =="
if ! command -v pkg >/dev/null 2>&1; then
  echo "请在原生 Termux 中运行此脚本，不支持在其他终端直接运行。"
  exit 1
fi

pkg update -y
pkg install -y python cmake ninja clang git curl tar xz-utils

mkdir -p "$INSTALL_DIR" "$NDK_PARENT"
cp "$SCRIPT_DIR/server.py" "$INSTALL_DIR/server.py"
chmod 700 "$INSTALL_DIR/server.py"

if ! find "$NDK_PARENT" -path '*/build/cmake/android.toolchain.cmake' -type f -print -quit | grep -q .; then
  echo
  echo "正在下载适配 ARM64 Termux 的 Android NDK r29。文件较大，首次安装需要等待。"
  curl -L --fail --retry 3 \
    "https://github.com/lzhiyong/termux-ndk/releases/download/android-ndk/android-ndk-r29-aarch64.tar.xz" \
    -o "$NDK_ARCHIVE"
  tar -xJf "$NDK_ARCHIVE" -C "$NDK_PARENT"
fi

TOOLCHAIN="$(find "$NDK_PARENT" -path '*/build/cmake/android.toolchain.cmake' -type f -print -quit)"
if [ -z "$TOOLCHAIN" ]; then
  echo "未在 NDK 中找到 build/cmake/android.toolchain.cmake。"
  echo "请检查 NDK 下载包或重新运行安装脚本。"
  exit 1
fi
NDK_ROOT="$(dirname "$(dirname "$(dirname "$TOOLCHAIN")")")"

printf 'export ANDROID_NDK_HOME=%q\n' "$NDK_ROOT" > "$INSTALL_DIR/env.sh"
printf 'export ANDROID_NDK_ROOT=%q\n' "$NDK_ROOT" >> "$INSTALL_DIR/env.sh"

if [ ! -d "$HOME/storage/shared" ]; then
  echo
  echo "首次运行需要授权 Android 共享存储："
  termux-setup-storage
fi

cat > "$INSTALL_DIR/start.sh" <<'EOF'
#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
source "$HOME/.plugin_factory_builder/env.sh"
exec python "$HOME/.plugin_factory_builder/server.py"
EOF
chmod 700 "$INSTALL_DIR/start.sh"

echo
echo "安装完成。Android NDK：$NDK_ROOT"
echo "1. 确保 Termux 已获 Android 共享存储权限。"
echo "2. 运行：$INSTALL_DIR/start.sh"
echo "3. 保持 Termux 服务运行，再切回 Godot。"
echo "4. 在 Godot 插件工厂中点击“测试 Termux 连接”。"
