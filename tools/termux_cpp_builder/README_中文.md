# Termux C++ 编译代理 0.1.0

## 作用
在 Android 手机的原生 Termux 中运行本地 HTTP 构建代理。Godot 编辑器通过
`127.0.0.1:8766` 请求编译和项目 ZIP 打包。代理只监听回环地址，并且只接受
Android 共享存储中的 Godot 项目目录。

## 安装
1. 在 Termux 中执行一次 `termux-setup-storage` 并允许文件访问。
2. 将项目放在 Android 共享存储，例如 `/storage/emulated/0/Documents/我的项目`。
3. 在 Termux 中进入本目录，执行：

```sh
bash setup.sh
```

安装脚本会安装 Python、CMake、Ninja、Clang、Git，并下载适配 Termux ARM64 的
Android NDK r29（首次下载较大，需要稳定网络和足够存储空间）。

4. 启动代理：

```sh
$HOME/.plugin_factory_builder/start.sh
```

5. 保持 Termux 会话运行，切回 Godot，打开“插件工厂”工作区并测试连接。

## 构建流程
1. 在 Godot 插件工厂点击“创建 C++ 扩展模板”。
2. 点击“测试 Termux 连接”，确认 NDK 工具链状态正常。
3. 点击“编译 C++ 扩展”。
4. 代理使用 NDK 的 `android.toolchain.cmake`，目标 `arm64-v8a`、API 24，
   生成真正的 Android ARM64 GDExtension。
5. 成功后生成 `bin/native/libchinese_native.so`。
6. Godot 项目根目录的 `chinese_native.gdextension` 指向该库。
7. 点击“打包整个项目 ZIP”，输出到 `dist/`。

## 技术边界
- 需要原生 Termux ARM64 与 Termux 适配的 Android NDK；不支持把 Linux x86_64
  动态库伪装成 Android 扩展。
- 项目 ZIP 是可移交的 Godot 项目源码包，包含已生成的 .so；不是 APK。
- APK 导出仍需 Godot Android 导出模板、Android SDK、NDK、JDK 与签名配置。
- 首次构建需要网络下载并编译 godot-cpp，耗时和内存占用较大。
- godot-cpp 使用 4.5 API 兼容基线，目标 Godot 运行环境为 4.7.2。
- Android Godot 编辑器能否加载设备上生成的扩展，仍需在实际设备验证。
