extends SceneTree

func _initialize() -> void:
    var plugin_script: Variant = load("res://addons/plugin_factory/plugin.gd")
    if plugin_script == null:
        push_error("Plugin Factory GDScript failed to load.")
        quit(1)
        return
    print("Plugin Factory GDScript parsed successfully.")
    quit(0)
