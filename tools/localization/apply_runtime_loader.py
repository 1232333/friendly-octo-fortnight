#!/usr/bin/env python3
"""Install an ASCII-only runtime localization bootstrap into the Orchestrator editor plugin."""
from pathlib import Path

ROOT_MARKER = 'set_meta("orchestrator_translation_root", true);'
LOADER_MARKER = 'res://addons/orchestrator/locale/translation_loader.gd'

def main() -> None:
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    args = parser.parse_args()
    source = args.root / "src/editor/plugins/orchestrator_editor_plugin.cpp"
    if not source.is_file():
        raise SystemExit(f"Orchestrator editor plugin source not found: {source}")

    text = source.read_text(encoding="utf-8")
    if ROOT_MARKER in text and LOADER_MARKER in text:
        print("Runtime localization bootstrap is already installed.")
        return

    anchor = "            _editor_panel = _main_screen->get_editor();\n            _main_screen->attach(this);"
    replacement = (
        "            _editor_panel = _main_screen->get_editor();\n"
        "            _editor_panel->set_meta(\"orchestrator_translation_root\", true);\n"
        "            _main_screen->attach(this);\n"
        "\n"
        "            // Load optional post-build localization resources without embedding translated text.\n"
        "            const Variant localization_script = ResourceLoader::get_singleton()->load(\n"
        "                \"res://addons/orchestrator/locale/translation_loader.gd\");\n"
        "            if (localization_script.get_type() == Variant::OBJECT) {\n"
        "                Node* localization_loader = memnew(Node);\n"
        "                localization_loader->set_script(localization_script);\n"
        "                add_child(localization_loader);\n"
        "            }\n"
    )
    if text.count(anchor) != 1:
        raise SystemExit(f"Expected exactly one Orchestrator plugin initialization anchor, found {text.count(anchor)}")
    text = text.replace(anchor, replacement)
    source.write_text(text, encoding="utf-8", newline="\n")

    if any(ord(char) > 127 for char in replacement):
        raise SystemExit("The injected C++ bootstrap must remain ASCII-only.")
    print("Installed ASCII-only runtime localization bootstrap; no translated strings were added to C++.")
if __name__ == "__main__":
    main()
