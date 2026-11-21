@tool
extends Node

const DOMAIN: StringName = &"addons/orchestrator"
const ROOT_META: StringName = &"orchestrator_translation_root"
const TRANSLATION_PATH := "res://addons/orchestrator/locale/zh_CN.po"
const REGULAR_FONT_PATH := "res://addons/orchestrator/fonts/NotoSansSC-Regular.otf"
const BOLD_FONT_PATH := "res://addons/orchestrator/fonts/NotoSansSC-Bold.otf"

var _font_cache: Dictionary = {}

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
    _install_font_fallbacks(root)
    root.propagate_notification(Node.NOTIFICATION_TRANSLATION_CHANGED)

func _find_translation_root(node: Node) -> Node:
    if node.has_meta(ROOT_META):
        return node
    for child in node.get_children():
        var found := _find_translation_root(child)
        if found != null:
            return found
    return null

func _install_font_fallbacks(node: Node) -> void:
    var regular_font := ResourceLoader.load(REGULAR_FONT_PATH) as Font
    var bold_font := ResourceLoader.load(BOLD_FONT_PATH) as Font
    if regular_font == null:
        push_warning("Orchestrator Chinese regular fallback font could not be loaded.")
        return

    _install_font_fallbacks_recursive(node, regular_font, bold_font)

func _install_font_fallbacks_recursive(node: Node, regular_font: Font, bold_font: Font) -> void:
    if node is Control:
        var control := node as Control
        var original_font := control.get_theme_font("font")
        if original_font is FontFile:
            var fallback_font := regular_font
            if bold_font != null and (original_font as FontFile).font_weight >= 600:
                fallback_font = bold_font
            var localized_font := _font_with_fallback(original_font as FontFile, fallback_font)
            if localized_font != null:
                control.add_theme_font_override("font", localized_font)

    for child in node.get_children():
        _install_font_fallbacks_recursive(child, regular_font, bold_font)

func _font_with_fallback(original_font: FontFile, fallback_font: Font) -> FontFile:
    var cache_key := str(original_font.get_instance_id()) + ":" + str(fallback_font.get_instance_id())
    if _font_cache.has(cache_key):
        return _font_cache[cache_key] as FontFile

    var localized_font := original_font.duplicate() as FontFile
    if localized_font == null:
        return null

    var fallbacks := localized_font.fallbacks
    if not fallbacks.has(fallback_font):
        fallbacks.append(fallback_font)
    localized_font.fallbacks = fallbacks
    _font_cache[cache_key] = localized_font
    return localized_font
