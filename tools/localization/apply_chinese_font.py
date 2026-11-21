#!/usr/bin/env python3
"""Bundle a subsetted Noto Sans CJK SC font and wire it into Orchestrator UI themes."""
import argparse
import shutil
import subprocess
from pathlib import Path

def make_font(source: Path, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([
        "pyftsubset", str(source), "--font-number=2",
        f"--output-file={output}",
        "--unicodes=U+0020-007E,U+00A0-00FF,U+2000-206F,U+3000-303F,U+4E00-9FFF,U+FF00-FFEF",
        "--layout-features=", "--name-IDs=0,1,2,4,6", "--name-languages=0x409",
        "--no-name-legacy", "--no-glyph-names", "--notdef-glyph", "--notdef-outline",
        "--recommended-glyphs",
    ], check=True)

def replace(path: Path, old: str, new: str, expected: int = 1, marker: str = "") -> None:
    text = path.read_text(encoding="utf-8")
    if marker and marker in text:
        return
    count = text.count(old)
    if count == 0 and new in text:
        return
    if count != expected:
        raise RuntimeError(f"{path}: expected {expected} matches, found {count}: {old!r}")
    path.write_text(text.replace(old, new), encoding="utf-8", newline="\n")

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--regular", required=True, type=Path)
    parser.add_argument("--bold", required=True, type=Path)
    parser.add_argument("--license", required=True, type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    addon = root / "project/addons/orchestrator"
    if not (root / "src/common/scene_utils.cpp").is_file() or not addon.is_dir():
        raise SystemExit(f"Not an Orchestrator source tree: {root}")

    fonts = addon / "fonts"
    if not (fonts / "NotoSansSC-Regular.otf").is_file():
        make_font(args.regular, fonts / "NotoSansSC-Regular.otf")
    if not (fonts / "NotoSansSC-Bold.otf").is_file():
        make_font(args.bold, fonts / "NotoSansSC-Bold.otf")
    shutil.copy2(args.license, fonts / "NotoSansCJK-LICENSE.txt")

    replace(root / "src/common/scene_utils.h",
        "    Ref<Font> get_editor_font(const String& p_font_name);\n",
        "    Ref<Font> get_editor_font(const String& p_font_name);\n\n"
        "    /// Get the bundled Simplified Chinese UI font, with editor-font fallback.\n"
        "    Ref<Font> get_orchestrator_font(bool p_bold = false);\n",
        marker="get_orchestrator_font(bool p_bold = false)")
    replace(root / "src/common/scene_utils.cpp",
        "    Ref<Font> get_editor_font(const String& p_font_name) {\n"
        "        return EI->get_editor_main_screen()->get_theme_font(p_font_name, \"EditorFonts\");\n"
        "    }\n",
        "    Ref<Font> get_editor_font(const String& p_font_name) {\n"
        "        return EI->get_editor_main_screen()->get_theme_font(p_font_name, \"EditorFonts\");\n"
        "    }\n\n"
        "    Ref<Font> get_orchestrator_font(bool p_bold) {\n"
        "        static Ref<Font> regular_font;\n"
        "        static Ref<Font> bold_font;\n"
        "        Ref<Font>& font = p_bold ? bold_font : regular_font;\n"
        "        if (font.is_null()) {\n"
        "            const String path = p_bold\n"
        "                ? \"res://addons/orchestrator/fonts/NotoSansSC-Bold.otf\"\n"
        "                : \"res://addons/orchestrator/fonts/NotoSansSC-Regular.otf\";\n"
        "            font = ResourceLoader::get_singleton()->load(path);\n"
        "        }\n"
        "        if (font.is_valid()) {\n"
        "            return font;\n"
        "        }\n"
        "        return get_editor_font(p_bold ? \"bold\" : \"main\");\n"
        "    }\n",
        marker="Ref<Font> get_orchestrator_font(bool p_bold)")
    replace(root / "src/editor/theme/theme_builder.cpp",
        "    const float base_margin = 4 * EDSCALE;\n",
        "    // Use the bundled Simplified Chinese font throughout Orchestrator controls.\n"
        "    const Ref<Font> ui_font = SceneUtils::get_orchestrator_font();\n"
        "    if (ui_font.is_valid()) {\n"
        "        p_theme->set_default_font(ui_font);\n"
        "    }\n\n"
        "    const float base_margin = 4 * EDSCALE;\n",
        marker="const Ref<Font> ui_font = SceneUtils::get_orchestrator_font();")
    replace(root / "src/editor/graph/graph_panel.cpp",
        '    _theme_cache.label_font = SceneUtils::get_editor_font("main_msdf");\n'
        '    _theme_cache.label_bold_font = SceneUtils::get_editor_font("main_bold_msdf");\n',
        '    _theme_cache.label_font = SceneUtils::get_orchestrator_font();\n'
        '    _theme_cache.label_bold_font = SceneUtils::get_orchestrator_font(true);\n')
    changes = {
        "src/editor/property_selector.cpp": [('SceneUtils::get_editor_font("source")', 'SceneUtils::get_orchestrator_font()', 2)],
        "src/editor/script_editor_view.cpp": [('SceneUtils::get_editor_font("main")', 'SceneUtils::get_orchestrator_font()', 2)],
        "src/editor/settings/input_event_configuration_dialog.cpp": [('SceneUtils::get_editor_font("bold")', 'SceneUtils::get_orchestrator_font(true)', 1)],
        "src/editor/gui/sectioned_inspector.cpp": [('SceneUtils::get_editor_font("bold")', 'SceneUtils::get_orchestrator_font(true)', 1)],
        "src/editor/gui/about_dialog.cpp": [('get_theme_font(StringName("source"), "EditorFonts")', 'SceneUtils::get_orchestrator_font()', 1)],
        "src/editor/doc/editor_help.cpp": [
            ('SceneUtils::get_editor_font("doc_bold")', 'SceneUtils::get_orchestrator_font(true)', 3),
            ('SceneUtils::get_editor_font("doc_italic")', 'SceneUtils::get_orchestrator_font()', 1),
            ('SceneUtils::get_editor_font("doc_source")', 'SceneUtils::get_orchestrator_font()', 1),
            ('SceneUtils::get_editor_font("doc_keyboard")', 'SceneUtils::get_orchestrator_font(true)', 1),
            ('SceneUtils::get_editor_font("doc")', 'SceneUtils::get_orchestrator_font()', 1),
        ],
    }
    for relative, items in changes.items():
        for old, new, count in items:
            replace(root / relative, old, new, count)

    for name in ("NotoSansSC-Regular.otf", "NotoSansSC-Bold.otf", "NotoSansCJK-LICENSE.txt"):
        asset = fonts / name
        if not asset.is_file() or asset.stat().st_size < 1000:
            raise RuntimeError(f"Missing/invalid font asset: {asset}")
    print("Bundled Noto Sans CJK SC regular + bold fonts; patched theme, graph, inspector, help and error UI.")

if __name__ == "__main__":
    main()
