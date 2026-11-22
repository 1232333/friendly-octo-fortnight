@tool
extends RefCounted
## 安全导入本地插件文件夹和 ZIP 插件包。

const EXCLUDED_DIRS := [".git", ".godot", "__pycache__", ".import"]


static func import_folder(source_path: String, addons_path: String) -> Dictionary:
    var source := source_path.strip_edges().replace("\\", "/").trim_suffix("/")
    if source.is_empty() or not DirAccess.dir_exists_absolute(source):
        return {"ok": false, "imported": [], "errors": ["所选路径不是可读取的文件夹。"]}

    var plugin_dirs: Array[String] = []
    _collect_plugin_dirs(source, plugin_dirs)
    if plugin_dirs.is_empty():
        return {"ok": false, "imported": [], "errors": ["所选文件夹及其子目录中没有找到 plugin.cfg。"]}

    var imported: Array[String] = []
    var errors: Array[String] = []
    for plugin_dir in plugin_dirs:
        var folder_name := _safe_folder_name(plugin_dir.get_file())
        var destination := addons_path.path_join(folder_name)
        if DirAccess.dir_exists_absolute(destination):
            errors.append("插件目录已存在，未覆盖：" + folder_name)
            continue

        var config := ConfigFile.new()
        if config.load(plugin_dir.path_join("plugin.cfg")) != OK:
            errors.append("无法读取插件配置：" + plugin_dir)
            continue

        var copy_error := _copy_directory(plugin_dir, destination)
        if copy_error.is_empty():
            imported.append(folder_name)
        else:
            _remove_directory(destination)
            errors.append("%s：%s" % [folder_name, copy_error])

    return {"ok": not imported.is_empty(), "imported": imported, "errors": errors}


static func import_zip(zip_path: String, addons_path: String) -> Dictionary:
    var reader := ZIPReader.new()
    var open_error: int = reader.open(zip_path)
    if open_error != OK:
        return {"ok": false, "imported": [], "errors": ["无法打开 ZIP 文件，错误码：%d" % open_error]}

    var entries: PackedStringArray = reader.get_files()
    var candidates: Array[Dictionary] = []
    var seen_roots: Dictionary = {}
    var errors: Array[String] = []

    for raw_entry in entries:
        var entry := _normalize_archive_path(raw_entry)
        if entry.is_empty() or not entry.ends_with("plugin.cfg"):
            continue
        if entry != "plugin.cfg" and not entry.ends_with("/plugin.cfg"):
            continue

        var root := entry.get_base_dir()
        if root == ".":
            root = ""
        if seen_roots.has(root):
            continue

        var config := ConfigFile.new()
        var config_data: PackedByteArray = reader.read_file(raw_entry)
        var parse_error: int = config.parse(config_data.get_string_from_utf8())
        if parse_error != OK:
            errors.append("插件配置格式错误：" + entry)
            continue

        var plugin_name := str(config.get_value("plugin", "name", ""))
        var folder_name := _safe_folder_name(root.get_file() if not root.is_empty() else plugin_name)
        if folder_name.is_empty():
            errors.append("无法确定插件目录名称：" + entry)
            continue

        seen_roots[root] = true
        candidates.append({"root": root, "folder": folder_name})

    if candidates.is_empty():
        reader.close()
        if errors.is_empty():
            errors.append("ZIP 中没有找到有效的 plugin.cfg，无法识别插件。")
        return {"ok": false, "imported": [], "errors": errors}

    var imported: Array[String] = []
    for candidate in candidates:
        var root: String = candidate["root"]
        var folder_name: String = candidate["folder"]
        var destination := addons_path.path_join(folder_name)
        if DirAccess.dir_exists_absolute(destination):
            errors.append("插件目录已存在，未覆盖：" + folder_name)
            continue

        var make_error: int = DirAccess.make_dir_recursive_absolute(destination)
        if make_error != OK and make_error != ERR_ALREADY_EXISTS:
            errors.append("无法创建插件目录：%s（%d）" % [folder_name, make_error])
            continue

        var prefix := root + "/" if not root.is_empty() else ""
        var extraction_failed := false
        for raw_entry in entries:
            var entry := _normalize_archive_path(raw_entry)
            if entry.is_empty() or entry.ends_with("/"):
                continue
            if not prefix.is_empty() and not entry.begins_with(prefix):
                continue

            var relative_path := entry.trim_prefix(prefix)
            if relative_path.is_empty() or not _is_safe_relative_path(relative_path):
                continue

            var target_path := destination.path_join(relative_path)
            var target_parent := target_path.get_base_dir()
            var parent_error: int = DirAccess.make_dir_recursive_absolute(target_parent)
            if parent_error != OK and parent_error != ERR_ALREADY_EXISTS:
                errors.append("无法创建目录：" + target_parent)
                extraction_failed = true
                break

            var data: PackedByteArray = reader.read_file(raw_entry)
            var output := FileAccess.open(target_path, FileAccess.WRITE)
            if output == null:
                errors.append("无法写入文件：" + relative_path)
                extraction_failed = true
                break
            output.store_buffer(data)
            output.close()

        if extraction_failed:
            _remove_directory(destination)
            continue

        if not FileAccess.file_exists(destination.path_join("plugin.cfg")):
            _remove_directory(destination)
            errors.append("ZIP 插件缺少根目录 plugin.cfg：" + folder_name)
            continue

        imported.append(folder_name)

    reader.close()
    return {"ok": not imported.is_empty(), "imported": imported, "errors": errors}


static func _collect_plugin_dirs(path: String, result: Array[String]) -> void:
    if FileAccess.file_exists(path.path_join("plugin.cfg")):
        result.append(path)
        return

    var dir := DirAccess.open(path)
    if dir == null:
        return
    dir.list_dir_begin()
    var entry := dir.get_next()
    while not entry.is_empty():
        if entry.begins_with(".") or entry in EXCLUDED_DIRS:
            entry = dir.get_next()
            continue
        if dir.current_is_dir():
            _collect_plugin_dirs(path.path_join(entry), result)
        entry = dir.get_next()
    dir.list_dir_end()


static func _copy_directory(source: String, destination: String) -> String:
    var make_error: int = DirAccess.make_dir_recursive_absolute(destination)
    if make_error != OK and make_error != ERR_ALREADY_EXISTS:
        return "无法创建目标目录（%d）" % make_error

    var dir := DirAccess.open(source)
    if dir == null:
        return "无法读取源目录"

    dir.list_dir_begin()
    var entry := dir.get_next()
    while not entry.is_empty():
        if entry.begins_with(".") or entry in EXCLUDED_DIRS or entry.ends_with(".tmp"):
            entry = dir.get_next()
            continue

        var source_path := source.path_join(entry)
        var destination_path := destination.path_join(entry)
        if dir.current_is_dir():
            var nested_error := _copy_directory(source_path, destination_path)
            if not nested_error.is_empty():
                dir.list_dir_end()
                return nested_error
        else:
            var input := FileAccess.open(source_path, FileAccess.READ)
            if input == null:
                dir.list_dir_end()
                return "无法读取文件：" + source_path
            var bytes := input.get_buffer(input.get_length())
            input.close()

            var output := FileAccess.open(destination_path, FileAccess.WRITE)
            if output == null:
                dir.list_dir_end()
                return "无法写入文件：" + destination_path
            output.store_buffer(bytes)
            output.close()

        entry = dir.get_next()
    dir.list_dir_end()
    return ""


static func _remove_directory(path: String) -> void:
    var dir := DirAccess.open(path)
    if dir == null:
        return
    dir.list_dir_begin()
    var entry := dir.get_next()
    while not entry.is_empty():
        if entry != "." and entry != "..":
            var child := path.path_join(entry)
            if dir.current_is_dir():
                _remove_directory(child)
            else:
                DirAccess.remove_absolute(child)
        entry = dir.get_next()
    dir.list_dir_end()
    DirAccess.remove_absolute(path)


static func _normalize_archive_path(path: String) -> String:
    var normalized := path.replace("\\", "/")
    if normalized.begins_with("/") or normalized.contains(":"):
        return ""
    for segment in normalized.split("/"):
        if segment == "..":
            return ""
    return normalized.trim_prefix("./")


static func _is_safe_relative_path(path: String) -> bool:
    if path.is_empty() or path.begins_with("/") or path.contains(":"):
        return false
    for segment in path.split("/"):
        if segment == ".." or segment.is_empty():
            return false
    return true


static func _safe_folder_name(value: String) -> String:
    var result := value.strip_edges()
    result = result.replace("/", "_").replace("\\", "_").replace(":", "_")
    result = result.replace("..", "_")
    return result
