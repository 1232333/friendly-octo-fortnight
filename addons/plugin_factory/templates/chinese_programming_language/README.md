# 中文编程语言 Godot 插件

这是一个可安装到 Godot 的 EditorPlugin 插件封装。C++ 编译器通过 GDExtension 提供 `ChineseLanguage` 类。

## 安装

1. 将本目录复制到项目的 `addons/chinese_programming_language`。
2. 确保目录内有 `bin/android/libchinese_native.so`（由 Plugin Factory 的 Termux 编译按钮生成）。
3. 在 Godot 项目设置的插件页面启用“中文编程语言”。

## 使用

打开 Godot 主屏幕中的“中文编程语言”页面，输入中文代码，点击“编译并保存 .zh → .gd”。默认保存：
- 中文源文件：`res://scripts/hello.zh`
- 生成脚本：`res://generated/hello.gd`

当前版本是中文语言前端 MVP：C++ 实现词法/行级转换，生成 GDScript；不是原生 C++ 语义代码生成器，也不是完整 AST 语言。
