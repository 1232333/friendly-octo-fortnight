@tool
extends EditorPlugin

const FACTORY_VERSION := "0.1.0"
const OUTPUT_PATH := "user://PluginFactory_Output.zip"
const EXCLUDED_DIRS := [".git", ".godot", "__pycache__", ".import"]

var panel: VBoxContainer
var plugin_list: ItemList
var log_view: RichTextLabel
var status_label: Label
var discovered_plugins: Array[Dictionary] = []


func _enter_tree() -> void:
    _create_interface()
    add_control_to_dock(DOCK_SLOT_RIGHT_UL, panel)
    _scan_plugins()


func _exit_tree() -> void:
    if is_instance_valid(panel):
        remove_control_from_docks(panel)
        panel.queue_free()


func _create_interface() -> void:
    panel = VBoxContainer.new()
    panel.name = "插件工厂"
    panel.custom_minimum_size = Vector2(330, 480)

    var title := Label.new()
    title.text = "插件工厂 Plugin Factory"
    title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    panel.add_child(title)

    var version := Label.new()
    version.text = "版本 " + FACTORY_VERSION
    panel.add_child(version)

    var scan_button := Button.new()
    scan_button.text = "扫描项目插件"
    scan_button.pressed.connect(_scan_plugins)
    panel.add_child(scan_button)

    var select_button := Button.new()
    select_button.text = "全选插件"
    select_button.pressed.connect(_select_all)
    panel.add_child(select_button)

    plugin_list = ItemList.new()
    plugin_list.select_mode = ItemList.SELECT_MULTI
    plugin_list.custom_minimum_size = Vector2(0, 190)
    panel.add_child(plugin_list)

    var combine_button := Button.new()
    combine_button.text = "检查并组合插件"
    combine_button.pressed.connect(_combine_selected)
    panel.add_child(combine_button)

    status_label = Label.new()
    status_label.text = "等待操作"
    panel.add_child(status_label)

    var log_title := Label.new()
    log_title.text = "操作日志"
    panel.add_child(log_title)

    log_view = RichTextLabel.new()
    log_view.custom_minimum_size = Vector2(0, 170)
    log_view.scroll_following = true
    panel.add_child(log_view)


func _log(message: String) -> void:
    if is_instance_valid(log_view):
        log_view.append_text(message + "\n")


func _scan_plugins() -> void:
    plugin_list.clear()
    discovered_plugins.clear()

    var root := "res://addons"
    if not DirAccess.dir_exists_absolute(ProjectSettings.globalize_path(root)):
        status_label.text = "未找到 res://addons 目录"
        return

    _scan_directory(root)

    for plugin in discovered_plugins:
        var index := plugin_list.add_item(
            "%s  |  %s" % [plugin.name, plugin.version]
        )
        plugin_list.set_item_metadata(index, plugin)

    status_label.text = "扫描完成：%d 个插件" % discovered_plugins.size()
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
    var error := config.load(cfg_path)
    if error != OK:
        _log("配置读取失败：" + cfg_path)
        return

    var dependencies: Variant = config.get_value(
        "plugin_factory", "dependencies", []
    )
    if not dependencies is Array and not dependencies is PackedStringArray:
        dependencies = []

    discovered_plugins.append({
        "name": str(config.get_value("plugin", "name", directory.get_file())),
        "version": str(config.get_value("plugin", "version", "未知")),
        "directory": directory,
        "dependencies": dependencies
    })


func _select_all() -> void:
    for i in range(plugin_list.item_count):
        plugin_list.select(i, false)
    status_label.text = "已选择全部插件"


func _get_selected_plugins() -> Array[Dictionary]:
    var result: Array[Dictionary] = []
    for index in plugin_list.get_selected_items():
        var item: Variant = plugin_list.get_item_metadata(index)
        if item is Dictionary:
            result.append(item)
    return result


func _check_dependencies(selected: Array[Dictionary]) -> bool:
    var selected_names: Array[String] = []
    for plugin in selected:
        selected_names.append(str(plugin.directory.get_file()))

    var valid := true
    for plugin in selected:
        for dependency in plugin.dependencies:
            if str(dependency) not in selected_names:
                _log("依赖缺失：%s 需要 %s" % [plugin.name, str(dependency)])
                valid = false
    return valid


func _combine_selected() -> void:
    var selected := _get_selected_plugins()
    if selected.is_empty():
        status_label.text = "请先选择插件"
        return

    _log("开始检查所选插件")
    if not _check_dependencies(selected):
        status_label.text = "依赖不完整，请查看日志"
        return

    var zip := ZIPWriter.new()
    var output_absolute := ProjectSettings.globalize_path(OUTPUT_PATH)
    var error := zip.open(output_absolute)
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
        _log("正在打包：" + plugin.name)
        manifest.plugins.append({
            "name": plugin.name,
            "version": plugin.version,
            "directory": plugin.directory.get_file()
        })

        var destination := "addons/" + str(plugin.directory.get_file())
        if not _pack_directory(zip, plugin.directory, destination):
            success = false
            break

    if success:
        var manifest_error := zip.start_file("plugin_factory_manifest.json")
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


func _pack_directory(zip: ZIPWriter, source: String, destination: String) -> bool:
    var dir := DirAccess.open(source)
    if dir == null:
        _log("无法打开目录：" + source)
        return false

    dir.list_dir_begin()
    var entry := dir.get_next()

    while entry != "":
        if entry in EXCLUDED_DIRS or entry.ends_with(".tmp"):
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

            var zip_error := zip.start_file(destination_path.replace("\\", "/"))
            if zip_error != OK:
                _log("ZIP 写入失败：" + destination_path)
                dir.list_dir_end()
                return false

            zip.write_file(data)
            zip.close_file()

        entry = dir.get_next()

    dir.list_dir_end()
    return true
