# Godot 插件工厂 0.4.0

## 安装
1. 将本目录复制到 Godot 项目的 `res://addons/plugin_factory/`。
2. 在 Godot 编辑器中打开“项目 -> 项目设置 -> 插件”。
3. 启用“插件工厂”。
4. 在 Godot 顶部主工作区切换到“插件工厂”页面标签。

## 0.4.0 更新
- **Termux C++ 编译代理**：通过本机回环地址连接 Android 原生 Termux。
- **C++ GDExtension 模板**：一键创建 godot-cpp CMake 工程、C++ 类和 `.gdextension` 声明。
- **Android ARM64 编译**：使用 Termux 适配 NDK r29 工具链、CMake 和 Ninja 编译 Android 动态库，输出到 `bin/native/libchinese_native.so`。
- **项目 ZIP 打包**：打包 Godot 项目源码和生成的动态库，排除 `.godot`、`.git` 与中间构建缓存。
- **安全边界**：编译代理仅监听 `127.0.0.1:8766`，编译/打包要求本机密钥，并限制项目路径在 Android 共享存储中。

## 0.3.0 更新
- **自动补齐依赖**：从所选插件开始递归解析依赖，并按依赖优先顺序生成最终组合。
- **循环依赖检测**：发现 A 依赖 B、B 又依赖 A 等循环时停止检查和打包。
- **缺失依赖检测**：依赖目录不存在或没有被扫描到时，显示具体缺失项。
- **组合冲突检查**：对自动解析后的完整插件集合检查冲突。
- **组合方案管理**：可命名保存、加载和删除方案；方案保存在当前项目的 `user://PluginFactory/profiles.json`。
- **依赖完整打包**：打包时自动包含依赖插件，仍保留每个插件自己的 `addons/<插件目录>/` 路径和源码。
- **清单增强**：ZIP 内的 `plugin_factory_manifest.json` 记录插件版本、目录、依赖和冲突声明。

## 插件依赖与冲突声明
在插件自己的 `plugin.cfg` 中增加：

```ini
[plugin_factory]
dependencies=["other_addon_folder"]
conflicts=["incompatible_addon_folder"]
```

依赖和冲突填写 `res://addons/` 下插件的目录名。依赖会递归解析，不需要用户手动逐层选择。

## 使用流程
1. 扫描项目插件。
2. 选择一个或多个需要的插件。
3. 点击“自动补齐依赖并检查冲突”。
4. 可输入方案名称并保存，之后可随时加载或删除。
5. 点击“解析依赖并打包组合 ZIP”。
6. 输出文件为 `user://PluginFactory_Output.zip`。

## 兼容与限制
- 目标环境：Godot 4.7.2；当前包面向 Android ARM64 编辑器项目。
- 依赖/冲突以插件目录名为标识，且需要在 `[plugin_factory]` 中主动声明。
- 插件管理继续负责 GDScript 插件发现、组合与插件包打包；C++ 编译由独立 Termux 代理执行。
- C++ 编译通过 Termux ARM64 NDK r29 工具链执行；首次安装下载 NDK、首次构建下载 godot-cpp。
- 当前项目 ZIP 不是 APK；APK 导出仍需 Android 导出模板、SDK/NDK、JDK 和签名配置。
- godot-cpp 当前以 4.5 API 为兼容基线，目标 Godot 运行环境为 4.7.2。
- GitHub Actions 会解析 Godot 插件脚本、运行代理单元测试，并在 Linux CI 编译 C++ 示例扩展；这不等同于 Termux Android ARM64 真机编译或 Android 编辑器加载验证。

## Termux 编译代理安装
参阅 `tools/termux_cpp_builder/README_中文.md`，在原生 Termux 执行 `bash setup.sh`（安装时会下载 Android NDK r29），然后运行 `$HOME/.plugin_factory_builder/start.sh`。复制 Termux 显示的本机密钥并粘贴到 Godot 插件工厂的密钥框；密钥保存在 `user://`。然后测试连接、创建 C++ 模板、编译和打包。
