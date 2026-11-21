#!/usr/bin/env python3
"""Generate runtime PO translation and bundled CJK fonts after the native build."""
import argparse
import json
import shutil
import subprocess
from pathlib import Path

UNICODES = "U+0020-007E,U+00A0-00FF,U+2000-206F,U+3000-303F,U+4E00-9FFF,U+FF00-FFEF"

def po_quote(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)

def write_po(translations: dict[str, str], output: Path) -> None:
    lines = [
        'msgid ""',
        'msgstr ""',
        '"Content-Type: text/plain; charset=UTF-8\\n"',
        '"Language: zh_CN\\n"',
        '"Plural-Forms: nplurals=1; plural=0;\\n"',
        "",
    ]
    for source, target in translations.items():
        if not source or not target:
            continue
        lines.extend((f"msgid {po_quote(source)}", f"msgstr {po_quote(target)}", ""))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="utf-8", newline="\n")

def make_font(source: Path, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([
        "pyftsubset", str(source), "--font-number=2",
        f"--output-file={output}", f"--unicodes={UNICODES}",
        "--layout-features=", "--name-IDs=0,1,2,4,6", "--name-languages=0x409",
        "--no-name-legacy", "--no-glyph-names", "--notdef-glyph", "--notdef-outline",
        "--recommended-glyphs",
    ], check=True)

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--translations", required=True, type=Path)
    parser.add_argument("--loader", required=True, type=Path)
    parser.add_argument("--regular", required=True, type=Path)
    parser.add_argument("--bold", required=True, type=Path)
    parser.add_argument("--license", required=True, type=Path)
    args = parser.parse_args()

    addon = args.root.resolve() / "project/addons/orchestrator"
    if not (addon / "orchestrator.gdextension").is_file():
        raise SystemExit(f"Orchestrator addon not found: {addon}")

    translations = json.loads(args.translations.read_text(encoding="utf-8"))
    locale_dir = addon / "locale"
    write_po(translations, locale_dir / "zh_CN.po")
    shutil.copy2(args.loader, locale_dir / "translation_loader.gd")

    fonts = addon / "fonts"
    make_font(args.regular, fonts / "NotoSansSC-Regular.otf")
    make_font(args.bold, fonts / "NotoSansSC-Bold.otf")
    shutil.copy2(args.license, fonts / "NotoSansCJK-LICENSE.txt")

    for path in (
        locale_dir / "zh_CN.po",
        locale_dir / "translation_loader.gd",
        fonts / "NotoSansSC-Regular.otf",
        fonts / "NotoSansSC-Bold.otf",
        fonts / "NotoSansCJK-LICENSE.txt",
    ):
        if not path.is_file() or path.stat().st_size == 0:
            raise RuntimeError(f"Missing runtime localization asset: {path}")
    if any(ord(char) > 127 for char in (locale_dir / "translation_loader.gd").read_text(encoding="utf-8")):
        raise RuntimeError("The runtime loader must remain ASCII-only.")
    print(f"Prepared {len(translations)} post-build translation entries and bundled CJK fonts.")

if __name__ == "__main__":
    main()
