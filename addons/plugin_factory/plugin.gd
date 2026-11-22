@tool
extends EditorPlugin

const FACTORY_VERSION := "0.5.0"
const PluginImporter = preload("res://addons/plugin_factory/importer.gd")
const OUTPUT_PATH := "user://PluginFactory_Output.zip"
const PROFILE_PATH := "user://PluginFactory/profiles.json"
const EXCLUDED_DIRS := [".git", ".godot", "__pycache__", ".import"]

var main_screen: ScrollContainer
var panel: VBoxContainer
var search_input: LineEdit
var plugin_list: ItemList
var details_view: RichTextLabel
var log_view: RichTextLabel
var status_label: Label
var profile_name_input: LineEdit
var profile_selector: OptionButton
var folder_dialog: FileDialog
var zip_dialog: FileDialog
var builder_request: HTTPRequest
var builder_status_label: Label
var builder_token_input: LineEdit
var builder_action := ""
var discovered_plugins: Array[Dictionary] = []
var visible_plugins: Array[Dictionary] = []
var saved_profiles: Array[Dictionary] = []


func _enter_tree() -> void:
    _create_interface()
    _load_builder_token()
    get_editor_interface().get_editor_main_screen().add_child(main_screen)
    saved_profiles = _read_profiles()
    _refresh_profile_selector()
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
    panel.custom_minimum_size = Vector2(420, 760)
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

    var import_row := HBoxContainer.new()
    panel.add_child(import_row)

    var import_folder_button := Button.new()
    import_folder_button.text = "导入本地插件文件夹"
    import_folder_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    import_folder_button.custom_minimum_size = Vector2(0, 44)
    import_folder_button.pressed.connect(_open_folder_dialog)
    import_row.add_child(import_folder_button)

    var import_zip_button := Button.new()
    import_zip_button.text = "导入 ZIP 插件包"
    import_zip_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    import_zip_button.custom_minimum_size = Vector2(0, 44)
    import_zip_button.pressed.connect(_open_zip_dialog)
    import_row.add_child(import_zip_button)

    folder_dialog = FileDialog.new()
    folder_dialog.title = "选择插件文件夹（其中必须包含 plugin.cfg）"
    folder_dialog.file_mode = FileDialog.FILE_MODE_OPEN_DIR
    folder_dialog.access = FileDialog.ACCESS_FILESYSTEM
    folder_dialog.dir_selected.connect(_import_folder)
    add_child(folder_dialog)

    zip_dialog = FileDialog.new()
    zip_dialog.title = "选择 ZIP 插件包"
    zip_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
    zip_dialog.access = FileDialog.ACCESS_FILESYSTEM
    zip_dialog.filters = PackedStringArray(["*.zip ; ZIP 插件包"])
    zip_dialog.file_selected.connect(_import_zip)
    add_child(zip_dialog)

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

    var profile_title := Label.new()
    profile_title.text = "组合方案管理"
    panel.add_child(profile_title)

    profile_name_input = LineEdit.new()
    profile_name_input.placeholder_text = "输入方案名称，例如：移动端基础组合"
    panel.add_child(profile_name_input)

    profile_selector = OptionButton.new()
    profile_selector.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    panel.add_child(profile_selector)

    var profile_buttons := HBoxContainer.new()
    panel.add_child(profile_buttons)

    var save_profile_button := Button.new()
    save_profile_button.text = "保存方案"
    save_profile_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    save_profile_button.custom_minimum_size = Vector2(0, 42)
    save_profile_button.pressed.connect(_save_profile)
    profile_buttons.add_child(save_profile_button)

    var load_profile_button := Button.new()
    load_profile_button.text = "加载方案"
    load_profile_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    load_profile_button.custom_minimum_size = Vector2(0, 42)
    load_profile_button.pressed.connect(_load_profile)
    profile_buttons.add_child(load_profile_button)

    var delete_profile_button := Button.new()
    delete_profile_button.text = "删除方案"
    delete_profile_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    delete_profile_button.custom_minimum_size = Vector2(0, 42)
    delete_profile_button.pressed.connect(_delete_profile)
    profile_buttons.add_child(delete_profile_button)

    var cpp_title := Label.new()
    cpp_title.text = "C++ 编译与项目打包（Termux）"
    panel.add_child(cpp_title)

    var cpp_help := Label.new()
    cpp_help.text = "创建可安装的中文编程语言 Godot 插件；由 Termux 编译 Android ARM64 C++ 核心，再打包独立插件 ZIP。"
    cpp_help.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    panel.add_child(cpp_help)

    var cpp_row_one := HBoxContainer.new()
    panel.add_child(cpp_row_one)

    var connection_button := Button.new()
    connection_button.text = "测试 Termux 连接"
    connection_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    connection_button.custom_minimum_size = Vector2(0, 44)
    connection_button.pressed.connect(_test_termux_connection)
    cpp_row_one.add_child(connection_button)

    var template_button := Button.new()
    template_button.text = "创建中文编程插件"
    template_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    template_button.custom_minimum_size = Vector2(0, 44)
    template_button.pressed.connect(_create_cpp_starter)
    cpp_row_one.add_child(template_button)

    var cpp_row_two := HBoxContainer.new()
    panel.add_child(cpp_row_two)

    var build_button := Button.new()
    build_button.text = "编译 C++ 扩展"
    build_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    build_button.custom_minimum_size = Vector2(0, 48)
    build_button.pressed.connect(_build_cpp_extension)
    cpp_row_two.add_child(build_button)

    var package_button := Button.new()
    package_button.text = "打包整个项目 ZIP"
    package_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    package_button.custom_minimum_size = Vector2(0, 48)
    package_button.pressed.connect(_package_project)
    cpp_row_two.add_child(package_button)

    var cpp_row_three := HBoxContainer.new()
    panel.add_child(cpp_row_three)
    var package_plugin_button := Button.new()
    package_plugin_button.text = "打包中文编程插件 ZIP"
    package_plugin_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    package_plugin_button.custom_minimum_size = Vector2(0, 48)
    package_plugin_button.pressed.connect(_package_chinese_plugin)
    cpp_row_three.add_child(package_plugin_button)

    builder_token_input = LineEdit.new()
    builder_token_input.placeholder_text = "粘贴 Termux 启动时显示的本机连接密钥"
    builder_token_input.secret = true
    panel.add_child(builder_token_input)

    builder_status_label = Label.new()
    builder_status_label.text = "Termux 编译代理：尚未连接（127.0.0.1:8766）"
    builder_status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    panel.add_child(builder_status_label)

    builder_request = HTTPRequest.new()
    builder_request.timeout = 0.0
    builder_request.use_threads = true
    builder_request.request_completed.connect(_on_builder_request_completed)
    add_child(builder_request)

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
    check_button.text = "自动补齐依赖并检查冲突"
    check_button.custom_minimum_size = Vector2(0, 44)
    check_button.pressed.connect(_check_selected)
    panel.add_child(check_button)

    var combine_button := Button.new()
    combine_button.text = "解析依赖并打包组合 ZIP"
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
    log_view.custom_minimum_size = Vector2(0, 135)
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
        "dependencies": _to_string_array(dependencies),
        "conflicts": _to_string_array(conflicts)
    })


func _to_string_array(value: Variant) -> Array[String]:
    var result: Array[String] = []
    if value is Array or value is PackedStringArray:
        for item in value:
            result.append(str(item).strip_edges())
    return result


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


func _build_plugin_index() -> Dictionary:
    var result: Dictionary = {}
    for plugin in discovered_plugins:
        result[str(plugin.get("folder", ""))] = plugin
    return result


func _visit_dependency(
    folder: String,
    plugin_index: Dictionary,
    visiting: Dictionary,
    visited: Dictionary,
    resolved: Array[Dictionary],
    issues: Array[String]
) -> void:
    if visited.has(folder):
        return
    if visiting.has(folder):
        issues.append("检测到循环依赖：" + folder)
        return
    if not plugin_index.has(folder):
        issues.append("找不到依赖插件目录：" + folder)
        return

    visiting[folder] = true
    var plugin: Dictionary = plugin_index[folder]
    for dependency in plugin.get("dependencies", []):
        var dependency_folder := str(dependency).strip_edges()
        if dependency_folder.is_empty():
            continue
        if not plugin_index.has(dependency_folder):
            issues.append("%s 缺少依赖：%s" % [str(plugin.get("name", folder)), dependency_folder])
        else:
            _visit_dependency(dependency_folder, plugin_index, visiting, visited, resolved, issues)

    visiting.erase(folder)
    visited[folder] = true
    resolved.append(plugin)


func _resolve_selection(selected: Array[Dictionary]) -> Dictionary:
    var plugin_index := _build_plugin_index()
    var visiting: Dictionary = {}
    var visited: Dictionary = {}
    var resolved: Array[Dictionary] = []
    var issues: Array[String] = []

    for plugin in selected:
        _visit_dependency(
            str(plugin.get("folder", "")),
            plugin_index,
            visiting,
            visited,
            resolved,
            issues
        )

    var selected_folders: Array[String] = []
    for plugin in resolved:
        selected_folders.append(str(plugin.get("folder", "")))

    for plugin in resolved:
        var plugin_name := str(plugin.get("name", "未命名"))
        for conflict in plugin.get("conflicts", []):
            var conflict_folder := str(conflict).strip_edges()
            if conflict_folder in selected_folders:
                issues.append("插件冲突：%s 与 %s 不能同时组合" % [plugin_name, conflict_folder])

    return {
        "ok": issues.is_empty(),
        "plugins": resolved,
        "issues": issues
    }


func _report_resolution(result: Dictionary) -> void:
    var issues: Array = result.get("issues", [])
    for issue in issues:
        _log(str(issue))
    if issues.is_empty():
        var resolved: Array = result.get("plugins", [])
        _log("依赖解析完成：最终组合包含 %d 个插件" % resolved.size())


func _check_selected() -> void:
    var selected := _get_selected_plugins()
    if selected.is_empty():
        status_label.text = "请先选择插件"
        return

    var result := _resolve_selection(selected)
    _report_resolution(result)
    if bool(result.get("ok", false)):
        status_label.text = "检查通过：已自动补齐依赖，共 %d 个插件" % result.get("plugins", []).size()
    else:
        status_label.text = "检查发现 %d 个问题，请查看操作日志" % result.get("issues", []).size()


func _save_profile() -> void:
    var profile_name := profile_name_input.text.strip_edges()
    if profile_name.is_empty():
        status_label.text = "请先输入组合方案名称"
        return

    var selected := _get_selected_plugins()
    if selected.is_empty():
        status_label.text = "请先选择要保存的插件"
        return

    var folders: Array[String] = []
    for plugin in selected:
        folders.append(str(plugin.get("folder", "")))

    var replaced := false
    for index in range(saved_profiles.size()):
        if str(saved_profiles[index].get("name", "")) == profile_name:
            saved_profiles[index] = {"name": profile_name, "plugins": folders}
            replaced = true
            break
    if not replaced:
        saved_profiles.append({"name": profile_name, "plugins": folders})

    if _write_profiles():
        _refresh_profile_selector()
        status_label.text = "组合方案已保存：" + profile_name
        _log("已保存方案：%s（%d 个插件）" % [profile_name, folders.size()])


func _load_profile() -> void:
    var index := profile_selector.selected
    if index < 0 or index >= saved_profiles.size():
        status_label.text = "请先选择一个已保存的方案"
        return

    var profile: Dictionary = saved_profiles[index]
    var wanted: Array = profile.get("plugins", [])
    search_input.clear()
    _apply_filter()

    plugin_list.deselect_all()
    var found := 0
    for item_index in range(visible_plugins.size()):
        var folder := str(visible_plugins[item_index].get("folder", ""))
        if folder in wanted:
            plugin_list.select(item_index)
            found += 1

    status_label.text = "已加载方案：%s（匹配 %d / %d 个插件）" % [
        str(profile.get("name", "")), found, wanted.size()
    ]
    _log("加载方案：%s" % str(profile.get("name", "")))
    if found < wanted.size():
        _log("部分插件未找到，请重新扫描或检查插件目录")


func _delete_profile() -> void:
    var index := profile_selector.selected
    if index < 0 or index >= saved_profiles.size():
        status_label.text = "请先选择一个已保存的方案"
        return

    var removed_name := str(saved_profiles[index].get("name", ""))
    saved_profiles.remove_at(index)
    if _write_profiles():
        _refresh_profile_selector()
        status_label.text = "已删除方案：" + removed_name
        _log("已删除方案：" + removed_name)


func _refresh_profile_selector() -> void:
    if not is_instance_valid(profile_selector):
        return
    profile_selector.clear()
    for profile in saved_profiles:
        profile_selector.add_item(str(profile.get("name", "未命名方案")))
    if not saved_profiles.is_empty():
        profile_selector.select(0)


func _read_profiles() -> Array[Dictionary]:
    var result: Array[Dictionary] = []
    if not FileAccess.file_exists(PROFILE_PATH):
        return result

    var file := FileAccess.open(PROFILE_PATH, FileAccess.READ)
    if file == null:
        return result

    var parsed: Variant = JSON.parse_string(file.get_as_text())
    file.close()
    if not (parsed is Dictionary):
        return result

    var entries: Variant = parsed.get("profiles", [])
    if entries is Array:
        for entry in entries:
            if entry is Dictionary and entry.has("name") and entry.has("plugins"):
                result.append({
                    "name": str(entry.get("name", "")),
                    "plugins": entry.get("plugins", [])
                })
    return result


func _write_profiles() -> bool:
    var directory := ProjectSettings.globalize_path("user://PluginFactory")
    var make_directory_error: int = DirAccess.make_dir_recursive_absolute(directory)
    if make_directory_error != OK and make_directory_error != ERR_ALREADY_EXISTS:
        _log("无法创建方案目录：" + str(make_directory_error))
        return false

    var file := FileAccess.open(PROFILE_PATH, FileAccess.WRITE)
    if file == null:
        _log("无法写入方案文件：" + str(FileAccess.get_open_error()))
        return false

    file.store_string(JSON.stringify({"version": 1, "profiles": saved_profiles}, "\t"))
    file.close()
    return true


func _combine_selected() -> void:
    var selected := _get_selected_plugins()
    if selected.is_empty():
        status_label.text = "请先选择插件"
        return

    _log("开始解析依赖并检查冲突")
    var resolution := _resolve_selection(selected)
    _report_resolution(resolution)
    if not bool(resolution.get("ok", false)):
        status_label.text = "依赖或冲突检查未通过，已停止打包"
        return

    var resolved: Array = resolution.get("plugins", [])
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
    for plugin in resolved:
        _log("正在打包：" + str(plugin.get("name", "未命名")))
        manifest["plugins"].append({
            "name": str(plugin.get("name", "")),
            "version": str(plugin.get("version", "")),
            "directory": str(plugin.get("folder", "")),
            "dependencies": plugin.get("dependencies", []),
            "conflicts": plugin.get("conflicts", [])
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
        status_label.text = "组合打包完成：%d 个插件" % resolved.size()
        _log("输出文件：" + OUTPUT_PATH)
        _log("共打包 %d 个插件（包含自动补齐的依赖）" % resolved.size())
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



func _open_folder_dialog() -> void:
    folder_dialog.popup_centered_ratio(0.85)


func _open_zip_dialog() -> void:
    zip_dialog.popup_centered_ratio(0.85)


func _import_folder(path: String) -> void:
    _run_import(PluginImporter.import_folder(
        path,
        ProjectSettings.globalize_path("res://addons")
    ), "文件夹")


func _import_zip(path: String) -> void:
    _run_import(PluginImporter.import_zip(
        path,
        ProjectSettings.globalize_path("res://addons")
    ), "ZIP")


func _run_import(result: Dictionary, source_type: String) -> void:
    var imported: Array = result.get("imported", [])
    var errors: Array = result.get("errors", [])
    for folder_name in imported:
        _log("已导入 %s 插件：%s" % [source_type, str(folder_name)])
    for issue in errors:
        _log("导入提示：" + str(issue))

    if imported.is_empty():
        status_label.text = "%s 导入未成功，请查看操作日志" % source_type
    else:
        _scan_plugins()
        status_label.text = "%s 导入完成：%d 个插件" % [source_type, imported.size()]



func _test_termux_connection() -> void:
    _request_builder("/health", "health", HTTPClient.METHOD_GET)


func _build_cpp_extension() -> void:
    _request_builder("/build", "build", HTTPClient.METHOD_POST)


func _package_project() -> void:
    _request_builder("/package", "package", HTTPClient.METHOD_POST)


func _package_chinese_plugin() -> void:
    _request_builder("/package_plugin", "package_plugin", HTTPClient.METHOD_POST)


func _request_builder(endpoint: String, action: String, method: int) -> void:
    if not is_instance_valid(builder_request):
        builder_status_label.text = "编译请求组件尚未初始化"
        return
    if builder_request.get_http_client_status() != HTTPClient.STATUS_DISCONNECTED:
        builder_status_label.text = "已有构建请求正在执行，请稍候"
        return

    builder_action = action
    builder_status_label.text = "正在连接 Termux 编译代理……"
    var url := "http://127.0.0.1:8766" + endpoint
    var headers := PackedStringArray()
    var body := ""
    if method == HTTPClient.METHOD_POST:
        var token := builder_token_input.text.strip_edges()
        if token.is_empty():
            builder_status_label.text = "请先粘贴 Termux 启动时显示的连接密钥"
            return
        headers.append("Content-Type: application/json")
        headers.append("X-Plugin-Factory-Token: " + token)
        _save_builder_token(token)
        body = JSON.stringify({
            "project_dir": ProjectSettings.globalize_path("res://").trim_suffix("/"),
            "jobs": 2
        })

    var error := builder_request.request(url, headers, method, body)
    if error != OK:
        builder_status_label.text = "无法发送请求，错误码：%d。请确认 Termux 服务已启动。" % error
        _log("Termux 请求创建失败，错误码：" + str(error))


func _on_builder_request_completed(
    result: int,
    response_code: int,
    _headers: PackedStringArray,
    body: PackedByteArray
) -> void:
    var response_text := body.get_string_from_utf8()
    var parsed: Variant = JSON.parse_string(response_text)
    if result != HTTPRequest.RESULT_SUCCESS:
        builder_status_label.text = "连接失败。请在 Termux 启动编译代理，并确认 Godot 与 Termux 位于同一设备。"
        _log("Termux HTTP 请求失败，结果码：" + str(result))
        return

    if not (parsed is Dictionary):
        builder_status_label.text = "编译代理返回了无法识别的数据"
        _log(response_text)
        return

    var data: Dictionary = parsed
    var ok := bool(data.get("ok", false))
    if builder_action == "health":
        var versions: Dictionary = data.get("versions", {})
        builder_status_label.text = str(data.get("message", "已收到响应"))
        _log("Termux 连接：" + str(data.get("message", "")))
        _log("clang：" + str(versions.get("clang", "未安装")))
        _log("CMake：" + str(versions.get("cmake", "未安装")))
        _log("Ninja：" + str(versions.get("ninja", "未安装")))
        _log("clang 目标：" + str(data.get("clang_target", "未知")))
        if not ok:
            builder_status_label.text += "（环境未就绪）"
        return

    if not ok or response_code < 200 or response_code >= 300:
        var failure := str(data.get("error", data.get("message", "操作失败")))
        builder_status_label.text = "操作失败：" + failure
        _log("Termux 操作失败：" + failure)
        if data.has("log"):
            _log("构建日志：" + str(data["log"]))
        return

    if builder_action == "build":
        get_editor_interface().get_resource_filesystem().scan()
        builder_status_label.text = "中文编程 C++ 核心编译完成，已安装到插件目录：" + str(data.get("library", ""))
        _log("Android ARM64 动态库已生成：" + str(data.get("library", "")))
        _log("动态库大小：" + str(data.get("size_bytes", 0)) + " bytes")
    elif builder_action == "package":
        builder_status_label.text = "项目 ZIP 已生成：" + str(data.get("package", ""))
        _log("项目打包完成：" + str(data.get("package", "")))
        _log("打包文件数：" + str(data.get("file_count", 0)))
    elif builder_action == "package_plugin":
        builder_status_label.text = "中文编程插件 ZIP 已生成：" + str(data.get("package", ""))
        _log("中文编程插件独立打包完成：" + str(data.get("package", "")))
        _log("插件文件数：" + str(data.get("file_count", 0)))
    builder_action = ""


func _create_cpp_starter() -> void:
    var project_root := ProjectSettings.globalize_path("res://")
    var cpp_root := project_root.path_join("cpp")
    var addon_root := project_root.path_join("addons/chinese_programming_language")
    if DirAccess.dir_exists_absolute(cpp_root) or DirAccess.dir_exists_absolute(addon_root):
        builder_status_label.text = "项目已有 cpp/ 或 addons/chinese_programming_language/，为防止覆盖未创建模板"
        return

    var addons_error := DirAccess.make_dir_recursive_absolute(project_root.path_join("addons"))
    if addons_error != OK and addons_error != ERR_ALREADY_EXISTS:
        builder_status_label.text = "无法创建 addons 目录，错误码：" + str(addons_error)
        return

    var cpp_error := _copy_template_directory("res://addons/plugin_factory/templates/cpp_extension", cpp_root)
    if not cpp_error.is_empty():
        builder_status_label.text = "创建 C++ 工程失败：" + cpp_error
        return

    var addon_error := _copy_template_directory("res://addons/plugin_factory/templates/chinese_programming_language", addon_root)
    if not addon_error.is_empty():
        builder_status_label.text = "创建 Godot 插件封装失败：" + addon_error
        return

    builder_status_label.text = "中文编程插件工程已创建：cpp/ + addons/chinese_programming_language/。接下来测试 Termux 连接并编译。"
    _log("已创建 C++ 编译工程和可启用的中文编程 Godot 插件封装")


func _copy_template_directory(source: String, destination: String) -> String:
    var absolute_source := ProjectSettings.globalize_path(source)
    var make_error := DirAccess.make_dir_recursive_absolute(destination)
    if make_error != OK and make_error != ERR_ALREADY_EXISTS:
        return "无法创建目录（%d）" % make_error

    var dir := DirAccess.open(absolute_source)
    if dir == null:
        return "无法读取内置 C++ 模板目录"
    dir.list_dir_begin()
    var entry := dir.get_next()
    while not entry.is_empty():
        if entry == "." or entry == "..":
            entry = dir.get_next()
            continue
        var source_path := absolute_source.path_join(entry)
        var destination_path := destination.path_join(entry)
        if dir.current_is_dir():
            var nested_error := _copy_template_directory(
                source.path_join(entry),
                destination_path
            )
            if not nested_error.is_empty():
                dir.list_dir_end()
                return nested_error
        else:
            var copy_error := DirAccess.copy_absolute(source_path, destination_path)
            if copy_error != OK:
                dir.list_dir_end()
                return "复制文件失败：%s（%d）" % [entry, copy_error]
        entry = dir.get_next()
    dir.list_dir_end()
    return ""



func _install_gdextension_descriptor() -> String:
    var source := ProjectSettings.globalize_path(
        "res://addons/plugin_factory/templates/cpp_extension/chinese_native.gdextension.template"
    )
    var destination := ProjectSettings.globalize_path("res://chinese_native.gdextension")
    if FileAccess.file_exists(destination):
        return ""
    var error := DirAccess.copy_absolute(source, destination)
    if error != OK:
        return "无法复制 chinese_native.gdextension（错误码 %d）" % error
    return ""



func _load_builder_token() -> void:
    if not FileAccess.file_exists("user://PluginFactory/termux_token.txt"):
        return
    var file := FileAccess.open("user://PluginFactory/termux_token.txt", FileAccess.READ)
    if file != null:
        builder_token_input.text = file.get_as_text().strip_edges()
        file.close()


func _save_builder_token(token: String) -> void:
    var directory := ProjectSettings.globalize_path("user://PluginFactory")
    var make_error := DirAccess.make_dir_recursive_absolute(directory)
    if make_error != OK and make_error != ERR_ALREADY_EXISTS:
        return
    var file := FileAccess.open("user://PluginFactory/termux_token.txt", FileAccess.WRITE)
    if file != null:
        file.store_string(token)
        file.close()
