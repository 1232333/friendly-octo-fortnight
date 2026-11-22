@tool
extends EditorPlugin

const FACTORY_VERSION := "0.2.1"
const OUTPUT_PATH := "user://PluginFactory_Output.zip"
const EXCLUDED_DIRS := [".git", ".godot", "__pycache__", ".import"]

var main_screen: ScrollContainer
var panel: VBoxContainer
var search_input: LineEdit
var plugin_list: ItemList
var details_view: RichTextLabel
var log_view: RichTextLabel
var status_label: Label
var discovered_plugins: Array[Dictionary] = []
var visible_plugins: Array[Dictionary] = []


func _enter_tree() -> void:
    _create_interface()
    get_editor_interface().get_editor_main_screen().add_child(main_screen)
    _scan_plugins()


func _exit_tree() -> void:
    if is_instance_valid(main_screen):
        if main_screen.get_parent() != null:
            main_screen.get_parent().remove_child(main_screen)
        main_screen.queue_free()


func _has_main_screen() -> bool:
    return true


func _get_plugin_name() -> String:
    return "插件工厂"


func _get_plugin_icon() -> Texture2D:
    return get_editor_interface().get_editor_theme().get_icon("Node", "EditorIcons")


func _make_visible(visible: bool) -> void:
    if is_instance_valid(main_screen):
        main_screen.visible = visible


func _create_interface() -> void:
    main_screen = ScrollContainer.new()
    main_screen.name = "插件工厂页面"
    main_screen.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    main_screen.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    main_screen.size_flags_vertical = Control.SIZE_EXPAND_FILL
    main_screen.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED

    panel = VBoxContainer.new()
    panel.name = "插件工厂内容"
    panel.custom_minimum_size = Vector2(420, 620)
    panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    main_screen.add_child(panel)

    var title := Label.new()
    title.text = "插件工厂 Plugin Factory"
    title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    panel.add_child(title)

    var version := Label.new()
    version.text = "版本 " + FACTORY_VERSION + "  |  Godot 4.7.2"
    panel.add_child(version)

    var scan_button := Button.new()
    scan_button.text = "重新扫描项目插件"
    scan_button.custom_minimum_size = Vector2(0, 44)
    scan_button.pressed.connect(_scan_plugins)
    panel.add_child(scan_button)

    search_input = LineEdit.new()
    search_input.placeholder_text = "搜索插件名称、目录或版本…"
    search_input.clear_button_enabled = true
    search_input.text_changed.connect(_apply_filter)
    panel.add_child(search_input)

    plugin_list = ItemList.new()
    plugin_list.select_mode = ItemList.SELECT_MULTI
    plugin_list.allow_reselect = true
    plugin_list.custom_minimum_size = Vector2(0, 220)
    plugin_list.item_selected.connect(_show_plugin_details)
    panel.add_child(plugin_list)

    var selection_row := HBoxContainer.new()
    panel.add_child(selection_row)

    var select_button := Button.new()
    select_button.text = "全选显示项"
    select_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    select_button.custom_minimum_size = Vector2(0, 42)
    select_button.pressed.connect(_select_all)
    selection_row.add_child(select_button)

    var clear_button := Button.new()
    clear_button.text = "清空选择"
    clear_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    clear_button.custom_minimum_size = Vector2(0, 42)
    clear_button.pressed.connect(_clear_selection)
    selection_row.add_child(clear_button)

    var details_title := Label.new()
    details_title.text = "插件详情"
    panel.add_child(details_title)

    details_view = RichTextLabel.new()
    details_view.custom_minimum_size = Vector2(0, 125)
    details_view.fit_content = true
    details_view.scroll_active = true
    details_view.text = "选择一个插件查看详细信息。"
    panel.add_child(details_view)

    var check_button := Button.new()
    check_button.text = "检查所选插件依赖与冲突"
    check_button.custom_minimum_size = Vector2(0, 44)
    check_button.pressed.connect(_check_selected)
    panel.add_child(check_button)

    var combine_button := Button.new()
    combine_button.text = "检查并组合插件 ZIP"
    combine_button.custom_minimum_size = Vector2(0, 48)
    combine_button.pressed.connect(_combine_selected)
    panel.add_child(combine_button)

    status_label = Label.new()
    status_label.text = "等待扫描"
    status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    panel.add_child(status_label)

    var log_title := Label.new()
    log_title.text = "操作日志"
    panel.add_child(log_title)

    log_view = RichTextLabel.new()
    log_view.custom_minimum_size = Vector2(0, 115)
    log_view.scroll_following = true
    log_view.scroll_active = true
    panel.add_child(log_view)


func _log(message: String) -> void:
    if is_instance_valid(log_view):
        log_view.append_text(message + "\n")


func _scan_plugins() -> void:
    plugin_list.clear()
    discovered_plugins.clear()
    visible_plugins.clear()
    details_view.text = "选择一个插件查看详细信息。"

    var root := "res://addons"
    if not DirAccess.dir_exists_absolute(ProjectSettings.globalize_path(root)):
        status_label.text = "未找到 res://addons 目录"
        return

    _scan_directory(root)
    _apply_filter(search_input.text)

    status_label.text = "扫描完成：发现 %d 个插件" % discovered_plugins.size()
    _log("扫描完成，发现 %d 个插件" % discovered_plugins.size())


func _scan_directory(path: String) -> void:
    var dir := DirAccess.open(path)
    if dir == null:
        _log("无法读取目录：" + path)
        return

    dir.list_dir_begin()
    var entry := dir.get_next()

    while entry != "":
        if entry.begins_with(".") or entry in EXCLUDED_DIRS:
            entry = dir.get_next()
            continue

        var child := path.path_join(entry)
        if dir.current_is_dir():
            var cfg_path := child.path_join("plugin.cfg")
            if FileAccess.file_exists(cfg_path):
                _read_plugin(cfg_path, child)
            _scan_directory(child)

        entry = dir.get_next()

    dir.list_dir_end()


func _read_plugin(cfg_path: String, directory: String) -> void:
    var config := ConfigFile.new()
    var error: int = config.load(cfg_path)
    if error != OK:
        _log("配置读取失败：" + cfg_path)
        return

    var dependencies: Variant = config.get_value("plugin_factory", "dependencies", [])
    var conflicts: Variant = config.get_value("plugin_factory", "conflicts", [])

    if not (dependencies is Array or dependencies is PackedStringArray):
        dependencies = []
    if not (conflicts is Array or conflicts is PackedStringArray):
        conflicts = []

    var folder_name := directory.get_file()
    for existing in discovered_plugins:
        if str(existing.get("directory", "")) == directory:
            return

    discovered_plugins.append({
        "name": str(config.get_value("plugin", "name", folder_name)),
        "version": str(config.get_value("plugin", "version", "未知")),
        "description": str(config.get_value("plugin", "description", "暂无描述")),
        "author": str(config.get_value("plugin", "author", "未知")),
        "directory": directory,
        "folder": folder_name,
        "config_path": cfg_path,
        "dependencies": dependencies,
        "conflicts": conflicts
    })


func _apply_filter(query: String = "") -> void:
    if not is_instance_valid(plugin_list):
        return

    plugin_list.clear()
    visible_plugins.clear()
    var needle := query.strip_edges().to_lower()

    for plugin in discovered_plugins:
        var searchable := "%s %s %s %s" % [
            str(plugin.get("name", "")),
            str(plugin.get("folder", "")),
            str(plugin.get("version", "")),
            str(plugin.get("description", ""))
        ]
        if needle.is_empty() or searchable.to_lower().contains(needle):
            visible_plugins.append(plugin)
            var index := plugin_list.add_item(
                "%s  |  %s" % [str(plugin.get("name", "未命名")), str(plugin.get("version", "未知"))]
            )
            plugin_list.set_item_metadata(index, plugin)

    status_label.text = "显示 %d / %d 个插件" % [visible_plugins.size(), discovered_plugins.size()]


func _show_plugin_details(index: int) -> void:
    if index < 0 or index >= visible_plugins.size():
        return

    var plugin: Dictionary = visible_plugins[index]
    var dependencies := PackedStringArray()
    var conflicts := PackedStringArray()
    for item in plugin.get("dependencies", []):
        dependencies.append(str(item))
    for item in plugin.get("conflicts", []):
        conflicts.append(str(item))

    details_view.text = (
        "名称：%s\n版本：%s\n作者：%s\n目录：%s\n说明：%s\n依赖：%s\n冲突：%s"
        % [
            str(plugin.get("name", "")),
            str(plugin.get("version", "")),
            str(plugin.get("author", "")),
            str(plugin.get("directory", "")),
            str(plugin.get("description", "")),
            "、".join(dependencies) if not dependencies.is_empty() else "无",
            "、".join(conflicts) if not conflicts.is_empty() else "无"
        ]
    )


func _select_all() -> void:
    for i in range(plugin_list.item_count):
        plugin_list.select(i)
    status_label.text = "已选择当前显示的 %d 个插件" % plugin_list.item_count


func _clear_selection() -> void:
    plugin_list.deselect_all()
    status_label.text = "已清空选择"


func _get_selected_plugins() -> Array[Dictionary]:
    var result: Array[Dictionary] = []
    for index in plugin_list.get_selected_items():
        var item: Variant = plugin_list.get_item_metadata(index)
        if item is Dictionary:
            result.append(item)
    return result


func _check_selected() -> void:
    var selected := _get_selected_plugins()
    if selected.is_empty():
        status_label.text = "请先选择插件"
        return

    var valid := _check_dependencies(selected)
    if valid:
        status_label.text = "检查通过：所选插件没有已声明的依赖缺失或冲突"
        _log("依赖与冲突检查通过")
    else:
        status_label.text = "检查发现问题，请查看操作日志"


func _check_dependencies(selected: Array[Dictionary]) -> bool:
    var selected_names: Array[String] = []
    for plugin in selected:
        selected_names.append(str(plugin.get("folder", "")))

    var valid := true
    for plugin in selected:
        var plugin_name := str(plugin.get("name", "未命名"))
        for dependency in plugin.get("dependencies", []):
            if str(dependency) not in selected_names:
                _log("依赖缺失：%s 需要 %s" % [plugin_name, str(dependency)])
                valid = false

        for conflict in plugin.get("conflicts", []):
            if str(conflict) in selected_names:
                _log("插件冲突：%s 与 %s 不能同时组合" % [plugin_name, str(conflict)])
                valid = false

    return valid


func _combine_selected() -> void:
    var selected := _get_selected_plugins()
    if selected.is_empty():
        status_label.text = "请先选择插件"
        return

    _log("开始检查所选插件")
    if not _check_dependencies(selected):
        status_label.text = "依赖或冲突检查未通过，请查看日志"
        return

    var zip: ZIPPacker = ZIPPacker.new()
    var output_absolute := ProjectSettings.globalize_path(OUTPUT_PATH)
    var error: int = zip.open(output_absolute)
    if error != OK:
        status_label.text = "ZIP 创建失败：" + str(error)
        _log("无法创建输出文件：" + output_absolute)
        return

    var manifest := {
        "name": "PluginFactoryBundle",
        "factory_version": FACTORY_VERSION,
        "target_godot": "4.7.2",
        "target_platform": "Android ARM64",
        "plugins": []
    }

    var success := true
    for plugin in selected:
        _log("正在打包：" + str(plugin.get("name", "未命名")))
        manifest["plugins"].append({
            "name": str(plugin.get("name", "")),
            "version": str(plugin.get("version", "")),
            "directory": str(plugin.get("folder", ""))
        })

        var destination := "addons/" + str(plugin.get("folder", ""))
        if not _pack_directory(zip, str(plugin.get("directory", "")), destination):
            success = false
            break

    if success:
        var manifest_error: int = zip.start_file("plugin_factory_manifest.json")
        if manifest_error == OK:
            zip.write_file(JSON.stringify(manifest, "\t").to_utf8_buffer())
            zip.close_file()
        else:
            _log("无法写入组合清单")
            success = false

    zip.close()

    if success:
        status_label.text = "组合打包完成"
        _log("输出文件：" + OUTPUT_PATH)
        _log("共打包 %d 个插件" % selected.size())
    else:
        status_label.text = "打包失败，请查看日志"


func _pack_directory(zip: ZIPPacker, source: String, destination: String) -> bool:
    var dir := DirAccess.open(source)
    if dir == null:
        _log("无法打开目录：" + source)
        return false

    dir.list_dir_begin()
    var entry := dir.get_next()

    while entry != "":
        if entry.begins_with(".") or entry in EXCLUDED_DIRS or entry.ends_with(".tmp"):
            entry = dir.get_next()
            continue

        var source_path := source.path_join(entry)
        var destination_path := destination.path_join(entry)

        if dir.current_is_dir():
            if not _pack_directory(zip, source_path, destination_path):
                dir.list_dir_end()
                return false
        else:
            var file := FileAccess.open(source_path, FileAccess.READ)
            if file == null:
                _log("读取失败：" + source_path)
                dir.list_dir_end()
                return false

            var data := file.get_buffer(file.get_length())
            file.close()

            var zip_error: int = zip.start_file(destination_path.replace("\\", "/"))
            if zip_error != OK:
                _log("ZIP 写入失败：" + destination_path)
                dir.list_dir_end()
                return false

            zip.write_file(data)
            zip.close_file()

        entry = dir.get_next()

    dir.list_dir_end()
    return true
