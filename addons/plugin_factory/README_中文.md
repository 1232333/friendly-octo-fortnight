# Godot 插件工厂 0.2.0

## 安装
1. 将本目录复制到 Godot 项目的 `res://addons/plugin_factory/`。
2. 在 Godot 编辑器中打开“项目 -> 项目设置 -> 插件”。
3. 启用“插件工厂”。

## 0.2.0 功能
- 扫描 `res://addons/` 下包含 `plugin.cfg` 的插件。
- 按名称、目录、版本和描述搜索插件。
- 显示插件名称、版本、作者、目录、描述、依赖和冲突信息。
- 多选插件，支持全选当前显示项和清空选择。
- 检查所选插件声明的依赖是否齐全。
- 检查所选插件之间声明的冲突。
- 保留各插件独立目录并组合为 ZIP。
- 生成 `plugin_factory_manifest.json`。

## 依赖与冲突声明
在插件的 `plugin.cfg` 中增加：

```ini
[plugin_factory]
dependencies=[\"other_addon_folder\"]
conflicts=[\"incompatible_addon_folder\"]
```

依赖和冲突名称填写 `res://addons/` 下对应插件的文件夹名。

## 输出
组合包写入 `user://PluginFactory_Output.zip`。可通过 Godot 的用户数据目录或后续版本的导出界面取出。

## 当前限制
- 依赖和冲突检查只识别插件主动声明的关系，不会自动分析 GDScript/C++ 的真实依赖。
- 当前组合包保留插件独立目录，不会把插件源码合并成一个文件夹。
- 本版本不执行 C++ 编译，也不自动安装组合包。
- GitHub 自动打包流程负责检查文件结构和 ZIP 完整性，不等同于 Godot 编辑器语法测试。
- 本次代码尚未经过用户 Android Godot 4.7.2 环境的 0.2.0 实机验证。
