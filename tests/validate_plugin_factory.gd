extends SceneTree

func _initialize() -> void:
    var importer_script: Variant = load("res://addons/plugin_factory/importer.gd")
    if importer_script == null:
        push_error("Plugin Factory importer GDScript failed to load.")
        quit(1)
        return

    var plugin_script: Variant = load("res://addons/plugin_factory/plugin.gd")
    if plugin_script == null:
        push_error("Plugin Factory GDScript failed to load.")
        quit(1)
        return

    if not FileAccess.file_exists("res://addons/plugin_factory/templates/cpp_extension/CMakeLists.txt"):
        push_error("C++ extension template is missing.")
        quit(1)
        return

    print("Plugin Factory 0.4.0 scripts parsed and C++ template found.")
    quit(0)
