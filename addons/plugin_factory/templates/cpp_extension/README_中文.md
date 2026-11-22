# C++ GDExtension 起步模板

由插件工厂创建。目标 Godot 4.7.2，Termux Android ARM64。

- `cpp/CMakeLists.txt`：通过 CMake 获取匹配 Godot 4.7 的 godot-cpp。
- `cpp/src/hello_extension.*`：示例 C++ 类。
- `cpp/src/register_types.cpp`：GDExtension 注册入口。
- `chinese_native.gdextension`：Android ARM64 动态库加载声明。
- 编译后动态库放在 `bin/native/libchinese_native.so`。

Godot 加载扩展后，可在 GDScript 中使用：

```gdscript
var native := HelloNative.new()
print(native.greeting())
```
