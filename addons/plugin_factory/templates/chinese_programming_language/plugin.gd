@tool
extends EditorPlugin

const PLUGIN_NAME := "中文编程语言"
const SOURCE_PATH := "res://scripts/hello.zh"
const OUTPUT_PATH := "res://generated/hello.gd"

var screen: VBoxContainer
var source_editor: TextEdit
var output_editor: TextEdit
var status_label: Label


func _enter_tree() -> void:
    _build_screen()
    get_editor_interface().get_editor_main_screen().add_child(screen)


func _exit_tree() -> void:
    if is_instance_valid(screen):
        if screen.get_parent():
            screen.get_parent().remove_child(screen)
        screen.queue_free()


func _has_main_screen() -> bool:
    return true


func _get_plugin_name() -> String:
    return PLUGIN_NAME


func _get_plugin_icon() -> Texture2D:
    return get_editor_interface().get_editor_theme().get_icon("Script", "EditorIcons")


func _make_visible(visible: bool) -> void:
    if is_instance_valid(screen):
        screen.visible = visible


func _build_screen() -> void:
    screen = VBoxContainer.new()
    screen.name = PLUGIN_NAME
    screen.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    screen.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    screen.size_flags_vertical = Control.SIZE_EXPAND_FILL

    var title := Label.new()
    title.text = "中文编程语言 0.1.0"
    title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    screen.add_child(title)

    var help := Label.new()
    help.text = "使用中文编写 .zh 脚本，点击编译后生成 Godot GDScript。"
    help.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    screen.add_child(help)

    source_editor = TextEdit.new()
    source_editor.size_flags_vertical = Control.SIZE_EXPAND_FILL
    source_editor.custom_minimum_size = Vector2(0, 220)
    source_editor.placeholder_text = "在这里输入中文程序……"
    source_editor.text = "变量 分数 = 80\n打印(分数)\n\n如果 分数 >= 60:\n    打印(\"及格\")\n否则:\n    打印(\"不及格\")\n结束\n"
    screen.add_child(source_editor)

    var compile_button := Button.new()
    compile_button.text = "编译并保存 .zh → .gd"
    compile_button.custom_minimum_size = Vector2(0, 48)
    compile_button.pressed.connect(_compile_source)
    screen.add_child(compile_button)

    status_label = Label.new()
    status_label.text = "等待编译"
    status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    screen.add_child(status_label)

    output_editor = TextEdit.new()
    output_editor.editable = false
    output_editor.size_flags_vertical = Control.SIZE_EXPAND_FILL
    output_editor.custom_minimum_size = Vector2(0, 180)
    output_editor.placeholder_text = "编译后的 GDScript 将显示在这里"
    screen.add_child(output_editor)


func _compile_source() -> void:
    if not ClassDB.class_exists("ChineseLanguage"):
        status_label.text = "C++ 编译器尚未加载。请先在插件工厂编译 Android ARM64 扩展，然后重启 Godot。"
        return

    var compiler: Object = ClassDB.instantiate("ChineseLanguage")
    if compiler == null:
        status_label.text = "无法创建 ChineseLanguage 编译器实例"
        return

    var result: Variant = compiler.call("compile", source_editor.text)
    compiler.free()
    if not (result is Dictionary):
        status_label.text = "编译器返回格式错误"
        return

    if not bool(result.get("ok", false)):
        status_label.text = "编译失败：" + str(result.get("error", "未知错误"))
        output_editor.text = ""
        return

    var generated := str(result.get("code", ""))
    var source_dir := ProjectSettings.globalize_path("res://scripts")
    var output_dir := ProjectSettings.globalize_path("res://generated")
    var source_error := DirAccess.make_dir_recursive_absolute(source_dir)
    var output_error := DirAccess.make_dir_recursive_absolute(output_dir)
    if source_error != OK and source_error != ERR_ALREADY_EXISTS:
        status_label.text = "无法创建 scripts 目录，错误码：" + str(source_error)
        return
    if output_error != OK and output_error != ERR_ALREADY_EXISTS:
        status_label.text = "无法创建 generated 目录，错误码：" + str(output_error)
        return

    var source_file := FileAccess.open(SOURCE_PATH, FileAccess.WRITE)
    if source_file == null:
        status_label.text = "无法保存中文源文件：" + SOURCE_PATH
        return
    source_file.store_string(source_editor.text)
    source_file.close()

    var output_file := FileAccess.open(OUTPUT_PATH, FileAccess.WRITE)
    if output_file == null:
        status_label.text = "无法写入编译结果：" + OUTPUT_PATH
        return
    output_file.store_string(generated)
    output_file.close()

    output_editor.text = generated
    get_editor_interface().get_resource_filesystem().scan()
    status_label.text = "编译成功！中文源码已保存到 %s，GDScript 已生成到 %s" % [SOURCE_PATH, OUTPUT_PATH]
