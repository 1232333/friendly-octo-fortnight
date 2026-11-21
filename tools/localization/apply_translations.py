#!/usr/bin/env python3
"""Apply reviewed Simplified-Chinese UI literals to Orchestrator source."""
from __future__ import annotations
import argparse, json, re
from pathlib import Path
from collections import Counter

STRING = re.compile(r'"([^"\\\n]*)"')
EXTS = {".cpp", ".h", ".gd", ".tscn", ".tres"}
SKIP = {".git", ".godot", "build", "bin", "obj", "extern", "thirdparty", "third_party", "vendor"}

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--translations", required=True)
    ap.add_argument("--report", required=True)
    args=ap.parse_args()
    root=Path(args.root).resolve()
    mapping=json.loads(Path(args.translations).read_text(encoding="utf-8"))
    hits=Counter()
    changed_files=[]
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in EXTS:
            continue
        rel=path.relative_to(root)
        if any(part in SKIP for part in rel.parts):
            continue
        # Only editor-facing code and the plugin's own UI resources are modified.
        if not (rel.parts[0]=="src" and len(rel.parts)>1 and rel.parts[1]=="editor") and not (rel.parts[0]=="project" and "addons" in rel.parts):
            continue
        try: original=path.read_text(encoding="utf-8")
        except (UnicodeDecodeError,OSError): continue
        def replace(m):
            value=m.group(1)
            translated=mapping.get(value)
            if translated is None: return m.group(0)
            hits[value]+=1
            return '"'+translated+'"'
        updated=STRING.sub(replace,original)
        if updated!=original:
            path.write_text(updated,encoding="utf-8",newline="")
            changed_files.append(rel.as_posix())
    report={
        "locale":"zh_CN",
        "translated_source_phrases":len(hits),
        "replaced_occurrences":sum(hits.values()),
        "changed_files":sorted(changed_files),
        "replacements":dict(sorted(hits.items())),
        "unmatched_translation_entries":sorted(set(mapping)-set(hits)),
    }
    Path(args.report).write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(f"Translated {sum(hits.values())} literal occurrences across {len(changed_files)} files.")
    print(f"Matched {len(hits)} unique translation entries.")
if __name__=="__main__": main()
