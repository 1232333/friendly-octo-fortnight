@tool
extends Node

const DOMAIN: StringName = &"addons/orchestrator"
const ROOT_META: StringName = &"orchestrator_translation_root"
const TRANSLATION_PATH := "res://addons/orchestrator/locale/zh_CN.po"
const FONT_PATH := "res://addons/orchestrator/fonts/NotoSansSC-Regular.otf"

func _enter_tree() -> void:
    call_deferred("_install_translation")

func _install_translation() -> void:
    var translation := ResourceLoader.load(TRANSLATION_PATH) as Translation
    if translation == null:
        push_warning("Orchestrator Chinese translation resource could not be loaded.")
        return

    var domain := TranslationServer.get_or_add_domain(DOMAIN)
    domain.add_translation(translation)
    domain.set_locale_override("zh_CN")

    var root := _find_translation_root(get_tree().root)
    if root == null:
        push_warning("Orchestrator translation root was not found.")
        return

    root.set_translation_domain(DOMAIN)
    root.set_auto_translate_mode(Node.AUTO_TRANSLATE_MODE_ALWAYS)
    _apply_font(root)
    root.propagate_notification(Node.NOTIFICATION_TRANSLATION_CHANGED)

func _find_translation_root(node: Node) -> Node:
    if node.has_meta(ROOT_META):
        return node
    for child in node.get_children():
        var found := _find_translation_root(child)
        if found != null:
            return found
    return null

func _apply_font(node: Node) -> void:
    var font := ResourceLoader.load(FONT_PATH) as Font
    if font == null:
        push_warning("Orchestrator Chinese UI font could not be loaded.")
        return
    _apply_font_recursive(node, font)

func _apply_font_recursive(node: Node, font: Font) -> void:
    if node is Control:
        var control := node as Control
        control.add_theme_font_override("font", font)
    for child in node.get_children():
        _apply_font_recursive(child, font)
