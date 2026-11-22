# Godot 插件工厂 0.1.0

## 安装
1. 将本目录复制到 Godot 项目的 `res://addons/plugin_factory/`。
2. 在 Godot 编辑器中打开“项目 -> 项目设置 -> 插件”。
3. 启用“插件工厂”。

## 当前原型功能
- 扫描 `res://addons/` 下包含 `plugin.cfg` 的插件。
- 多选插件。
- 检查插件声明的依赖。
- 保留插件独立目录并组合为 ZIP。
- 生成 `plugin_factory_manifest.json`。

## 依赖声明
在插件的 `plugin.cfg` 中声明：

```ini
[plugin_factory]
dependencies=["other_addon_folder"]
```

依赖名称填写 `res://addons/` 下的插件文件夹名。

## 说明
当前版本是原型，尚未在 Godot 4.7.2 Android ARM64 真机验证。
本版本尚不执行 C++ 编译，也不自动安装组合包。
