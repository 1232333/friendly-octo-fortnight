#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INSTALL_DIR="$HOME/.plugin_factory_builder"

echo "== Plugin Factory Termux C++ Builder setup =="
if ! command -v pkg >/dev/null 2>&1; then
  echo "请在原生 Termux 中运行此脚本，不支持在其他终端直接运行。"
  exit 1
fi

pkg update -y
pkg install -y python cmake ninja clang git

mkdir -p "$INSTALL_DIR"
cp "$SCRIPT_DIR/server.py" "$INSTALL_DIR/server.py"
chmod 700 "$INSTALL_DIR/server.py"

if [ ! -d "$HOME/storage/shared" ]; then
  echo
  echo "首次运行需要授权 Android 共享存储："
  termux-setup-storage
fi

cat > "$INSTALL_DIR/start.sh" <<'EOF'
#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
exec python "$HOME/.plugin_factory_builder/server.py"
EOF
chmod 700 "$INSTALL_DIR/start.sh"

echo
echo "安装完成。"
echo "1. 确保 Termux 已获文件访问权限。"
echo "2. 运行：$INSTALL_DIR/start.sh"
echo "3. 保持 Termux 服务运行，再切回 Godot。"
echo "4. 在 Godot 插件工厂中点击“测试 Termux 连接”。"
