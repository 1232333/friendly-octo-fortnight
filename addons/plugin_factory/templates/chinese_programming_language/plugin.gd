@tool
extends EditorPlugin

const PLUGIN_NAME := "中文编程语言"
const SOURCE_PATH := "res://scripts/hello.zh"
const BYTECODE_PATH := "res://generated/hello.zhbc"

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
    title.text = "中文原生编程语言 0.4.0 · ZHBC VM"
    title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    screen.add_child(title)
    var help := Label.new()
    help.text = "中文源码会编译为独立 .zhbc 字节码，由 C++ 虚拟机执行；不会生成 GDScript。"
    help.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    screen.add_child(help)
    source_editor = TextEdit.new()
    source_editor.size_flags_vertical = Control.SIZE_EXPAND_FILL
    source_editor.custom_minimum_size = Vector2(0, 220)
    source_editor.placeholder_text = "输入中文程序……"
    source_editor.text = _load_initial_source()
    screen.add_child(source_editor)
    var compile_button := Button.new()
    compile_button.text = "编译并运行（原生 VM）"
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
    output_editor.placeholder_text = "VM 执行输出 / 错误"
    screen.add_child(output_editor)

func _load_initial_source() -> String:
    if FileAccess.file_exists(SOURCE_PATH):
        var file := FileAccess.open(SOURCE_PATH, FileAccess.READ)
        if file != null:
            var saved_source := file.get_as_text()
            file.close()
            return saved_source
    return "变量 分数: 整数 = 80\\n打印(\\\"分数：\\\", 分数)\\n如果 分数 >= 60:\\n    打印(\\\"及格\\\")\\n否则:\\n    打印(\\\"不及格\\\")\\n结束\\n"


func _compile_source() -> void:
    if not ClassDB.class_exists("ChineseLanguage"):
        status_label.text = "原生中文 VM 尚未加载。请先编译并安装 C++ 扩展。"
        return
    var compiler: Object = ClassDB.instantiate("ChineseLanguage")
    if compiler == null:
        status_label.text = "无法创建原生中文语言实例"
        return
    var result: Variant = compiler.call("compile", source_editor.text)
    if not (result is Dictionary):
        status_label.text = "编译失败：原生编译器返回格式错误"
        output_editor.text = ""
        compiler = null
        return
    if not bool(result.get("ok", false)):
        status_label.text = "编译失败：" + str(result.get("error", "未知编译错误"))
        output_editor.text = ""
        compiler = null
        return
    var bytecode: PackedByteArray = result.get("bytecode", PackedByteArray())
    if bytecode.is_empty():
        status_label.text = "编译器没有生成有效的 ZHBC 字节码"
        compiler = null
        return
    var source_dir := ProjectSettings.globalize_path("res://scripts")
    var output_dir := ProjectSettings.globalize_path("res://generated")
    var e1 := DirAccess.make_dir_recursive_absolute(source_dir)
    var e2 := DirAccess.make_dir_recursive_absolute(output_dir)
    if e1 != OK and e1 != ERR_ALREADY_EXISTS:
        status_label.text = "无法创建 scripts 目录：" + str(e1)
        compiler = null
        return
    if e2 != OK and e2 != ERR_ALREADY_EXISTS:
        status_label.text = "无法创建 generated 目录：" + str(e2)
        compiler = null
        return
    var src := FileAccess.open(SOURCE_PATH, FileAccess.WRITE)
    if src == null:
        status_label.text = "无法保存中文源文件"
        compiler = null
        return
    src.store_string(source_editor.text)
    src.close()
    var out := FileAccess.open(BYTECODE_PATH, FileAccess.WRITE)
    if out == null:
        status_label.text = "无法写入 ZHBC 字节码"
        compiler = null
        return
    out.store_buffer(bytecode)
    out.close()
    var execution: Dictionary = compiler.call("run", bytecode, get_editor_interface().get_edited_scene_root())
    output_editor.text = str(execution.get("output", "")) + ("" if bool(execution.get("ok", false)) else str(execution.get("error", "执行失败")))
    status_label.text = "原生编译并执行成功 · ZHBC v2 · %d 字节" % bytecode.size() if bool(execution.get("ok", false)) else "VM 执行失败"
    compiler = null
    get_editor_interface().get_resource_filesystem().scan()
