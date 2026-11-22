# Godot 插件工厂 0.2.1

## 安装
1. 将本目录复制到 Godot 项目的 `res://addons/plugin_factory/`。
2. 在 Godot 编辑器中打开“项目 -> 项目设置 -> 插件”。
3. 启用“插件工厂”。
4. 在 Godot 顶部主工作区切换到“插件工厂”页面标签。

## 0.2.1 更新
- 将插件工厂改为 Godot 编辑器独立主工作区页面，不再占用右侧停靠栏。
- 使用可滚动页面布局，避免内容超出可视区域。
- 不再把操作日志作为 Godot 底部输出面板的替代品；Godot 原有输出、调试器等底部面板保持独立。
- 保留插件扫描、搜索、详情、多选、依赖/冲突检查和 ZIP 组合打包。

## 0.2.0 功能
- 扫描 `res://addons/` 下包含 `plugin.cfg` 的插件。
- 按名称、目录、版本和描述搜索插件。
- 显示插件名称、版本、作者、目录、描述、依赖和冲突。
- 多选、全选当前显示项和清空选择。
- 检查声明的依赖缺失和插件冲突。
- 保留插件独立目录并组合为 ZIP，生成 `plugin_factory_manifest.json`。

## 依赖与冲突声明
在插件的 `plugin.cfg` 中增加：

```ini
[plugin_factory]
dependencies=[\"other_addon_folder\"]
conflicts=[\"incompatible_addon_folder\"]
```

依赖和冲突名称填写 `res://addons/` 下对应插件的文件夹名。

## 输出
组合包写入 `user://PluginFactory_Output.zip`。

## 当前限制
- 依赖和冲突检查只识别插件主动声明的关系。
- 本版本不执行 C++ 编译，也不自动安装组合包。
- GitHub Actions 检查文件结构和 ZIP 完整性，不等同于 Godot GDScript 语法测试。
- 仍需在 Android Godot 4.7.2 环境验证页面显示和交互。
