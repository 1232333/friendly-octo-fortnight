# Termux C++ Builder

This local service is used by the Plugin Factory editor plugin.

## Install or update

From the Godot project root in Termux:

```bash
cd tools/termux_cpp_builder
bash setup.sh
$HOME/.plugin_factory_builder/start.sh
```

Re-running `setup.sh` updates the installed server script. If the Android NDK is already installed, the setup script reuses it instead of downloading it again.

## Chinese programming language addon flow

1. In Godot, open Plugin Factory and choose **创建中文编程插件**.
2. Test the Termux connection.
3. Choose **编译 C++ 扩展**. The Android ARM64 library is copied to:
   `addons/chinese_programming_language/bin/android/libchinese_native.so`
4. Choose **打包中文编程插件 ZIP**. The standalone installable addon is created at:
   `dist/ChineseProgrammingLanguage-Android-ARM64.zip`
5. Enable **中文编程语言** in Project > Project Settings > Plugins.

The service binds only to `127.0.0.1:8766` and requires the per-device token printed at startup. Keep Termux running while compiling or packaging.

The generated addon ZIP contains the complete `addons/chinese_programming_language/` path and can be extracted at the root of another Godot project. It is Android ARM64-specific.
