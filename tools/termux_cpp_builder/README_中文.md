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

4. 启动代理：

```sh
$HOME/.plugin_factory_builder/start.sh
```

5. 保持 Termux 会话运行，切回 Godot，打开“插件工厂”工作区并测试连接。

## 需要的 Termux 包
安装脚本会安装 `python`、`clang`、`cmake`、`ninja` 和 `git`。

## 构建流程
1. 在 Godot 插件工厂点击“创建 C++ 扩展模板”。
2. 点击“编译 C++ 扩展”。
3. 代理通过 Termux 原生 Android ARM64 clang 编译 `cpp/` 中的 GDExtension。
4. 成功后生成 `bin/native/libchinese_native.so`。
5. Godot 项目根目录的 `chinese_native.gdextension` 指向该库。
6. 点击“打包整个项目 ZIP”，输出到 `dist/`。

## 限制
- 当前目标是 Termux 原生环境的 Android ARM64 GDExtension，不是 Windows/Linux 交叉编译。
- 项目 ZIP 是可移交的 Godot 项目源码包，包含已生成的 .so；不是 APK。
- 导出 APK 仍需 Godot Android 导出模板、Android SDK/NDK、JDK 与签名配置。
- 首次构建需要网络下载与编译 godot-cpp，耗时和内存占用较大。
- Android Godot 编辑器是否能加载该设备上编译的扩展，仍需在实际设备验证。
