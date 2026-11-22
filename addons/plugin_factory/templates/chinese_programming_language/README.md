# 中文游戏编程语言 Godot 插件

这是面向 Godot 游戏节点的中文脚本插件。C++ GDExtension 提供中文编译器，编译结果由 Godot GDScript 运行时执行。

## 安装

1. 将本目录复制到项目的 `addons/chinese_programming_language`。
2. 确保 `bin/android/libchinese_native.so` 已由 Plugin Factory 的 Termux 编译流程生成。
3. 在 Godot 项目设置的插件页面启用“中文编程语言”。

## 使用

1. 在场景树中选中要控制的节点（例如 CharacterBody2D、Node2D 或 Control）。
2. 打开“中文编程语言”页面，编写中文脚本。
3. 点击“编译并绑定到选中节点”。
4. 编译器生成 GDScript，先由 Godot 检查，再通过编辑器撤销系统绑定到所选节点。
5. 保存场景并运行游戏。

示例：

```text
当 游戏开始:
    自身.position = Vector2(100, 100)
    打印("角色已生成")
结束

当 每帧(delta):
    如果 输入.正在按下("向右"):
        自身.position.x += 200 * delta
    结束
结束
```

当前已支持的游戏事件：`游戏开始`、`每帧(delta)`、`物理帧(delta)`、`输入事件(事件)`、`退出场景`。输入动作名称需要先在 Godot 项目的 Input Map 中配置。

常用中文 API 别名：`自身`、`输入`、`获取节点`、`获取节点或空`、`位置`、`旋转`、`缩放`、`可见`、`名称`、`正在按下`、`移动并滑动`。

## 当前边界

这是可执行的第一阶段，不是完整独立虚拟机。C++ 编译器目前采用行级语法并生成 GDScript；Godot 负责最终脚本检查和运行。完整 AST、静态类型系统、模块系统、调试映射、更多节点 API 和独立字节码仍需后续开发。Android 真机还需要重新编译更新后的 C++ 扩展并验证。
