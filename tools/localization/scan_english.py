#!/usr/bin/env python3
"""Audit likely user-facing English literals in Orchestrator source files."""
from __future__ import annotations
import argparse, csv, re
from pathlib import Path
from collections import Counter

EXTS={".gd",".tscn",".tres",".cpp",".cc",".c",".h",".hpp",".hh",".xml",".svg"}
SKIP_DIRS={".git",".godot","build","bin","obj","extern","thirdparty","third_party","vendor","node_modules","__pycache__","tests",".github"}
DOUBLE=re.compile(r'"((?:\\\\.|[^"\\\\])*)"')
SINGLE=re.compile(r"""(?<![A-Za-z0-9_])'((?:\\\\.|[^'\\\\])*)'""")
WORD=re.compile(r"[A-Za-z][A-Za-z'-]{1,}")
CODE_ONLY={"true","false","null","none","ok","yes","no","node","node2d","node3d","resource","script","editor","default","value","name","type","path","load","new","get","set","add","remove","update","reset"}
UI_HINTS=("ADD_","set_text","set_tooltip","set_title","set_placeholder","add_item","add_option","PROPERTY","EDITOR_SETTING","push_warning","push_error","ERR_","WARN_","display_","dialog","label","tooltip","description","title","text","hint","menu")

def decode_literal(s):
    # Decode common source escapes without corrupting existing UTF-8.
    return (s.replace(r"\\n","\n").replace(r"\\t","\t")
             .replace(r'\\"','"').replace(r"\\'","'")
             .replace(r"\\\\","\\")).strip()

def is_candidate(s):
    s=s.strip()
    if len(s)<2 or not re.search(r"[A-Za-z]",s): return False
    if len(s)>600 or "\n" in s: return False
    if re.search(r"(?:res|uid|user|https?)://|\$\{|\$[A-Z_]+|^[./\\]|\\.(?:h|cpp|gd|png|svg|so)$",s,re.I): return False
    if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_:.\/-]*",s): return False
    if re.fullmatch(r"[A-Z0-9_ .:-]+",s): return False
    words=[w.lower() for w in WORD.findall(s)]
    if not words or (len(words)==1 and words[0] in CODE_ONLY): return False
    # Ignore build flags, version hashes, command fragments, and format-only tokens.
    if re.search(r"(^|\s)(--?[A-Za-z0-9_-]+|\$\{[^}]+\}|#[0-9A-Fa-f]{3,})",s): return False
    if re.search(r"\b(?:cmake|python|pip|git|clang|gcc|ninja|scons|https?)\b",s,re.I) and len(words)<5: return False
    return len(words)>=2 or (len(s)>=14 and " " in s)

def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--root",required=True); ap.add_argument("--out",required=True)
    a=ap.parse_args(); root=Path(a.root).resolve(); out=Path(a.out).resolve(); out.mkdir(parents=True,exist_ok=True)
    rows=[]; seen=set()
    for p in sorted(root.rglob("*")):
        if not p.is_file() or p.suffix.lower() not in EXTS: continue
        rel=p.relative_to(root)
        if any(x in SKIP_DIRS for x in rel.parts): continue
        if not any(x in rel.parts for x in ("src","project","doc_classes","docs","include")): continue
        try: lines=p.read_text(encoding="utf-8-sig").splitlines()
        except (UnicodeDecodeError,OSError): continue
        for n,line in enumerate(lines,1):
            matches=list(DOUBLE.finditer(line))+list(SINGLE.finditer(line))
            for m in matches:
                value=decode_literal(m.group(1))
                if not is_candidate(value): continue
                key=(value,rel.as_posix(),n)
                if key in seen: continue
                seen.add(key)
                context=line.strip()[:300]
                hints=[h for h in UI_HINTS if h.lower() in context.lower()]
                rows.append({"source_file":rel.as_posix(),"line":n,"source_text":value,
                             "ui_context":";".join(hints),"translation_zh_CN":"","review_status":"needs_translation"})
    fields=["source_file","line","source_text","ui_context","translation_zh_CN","review_status"]
    with (out/"english_strings.csv").open("w",newline="",encoding="utf-8-sig") as f:
        w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(rows)
    unique={}
    for r in rows: unique.setdefault(r["source_text"],[]).append(f'{r["source_file"]}:{r["line"]}')
    def q(s): return '"'+s.replace("\\","\\\\").replace('"','\\"').replace("\n","\\n")+'"'
    pot=['msgid ""','msgstr ""','"Content-Type: text/plain; charset=UTF-8\\n"','"Content-Transfer-Encoding: 8bit\\n"','"Language: zh_CN\\n"','']
    for msg,ctx in sorted(unique.items()): pot += ["#: "+" ".join(ctx[:20]),"msgid "+q(msg),'msgstr ""',""]
    (out/"orchestrator_zh_CN.pot").write_text("\n".join(pot),encoding="utf-8")
    ext=Counter(Path(r["source_file"]).suffix.lower() for r in rows)
    summary=["# Orchestrator English-string audit","",f"- Candidate occurrences: **{len(rows)}**",f"- Unique source strings: **{len(unique)}**",f"- Source files containing candidates: **{len(set(r['source_file'] for r in rows))}**","","## By file type","","| Extension | Occurrences |","|---|---:|"]
    summary += [f"| `{k}` | {v} |" for k,v in sorted(ext.items())]
    summary += ["","## Output","","- `english_strings.csv`: source, line, candidate text, UI-context hints, Simplified Chinese field, review status.","- `orchestrator_zh_CN.pot`: gettext translation template.","","Only source, addon, and class-documentation areas are scanned. Test fixtures, CI workflows, build presets, dependencies, identifiers, paths, and source files are not rewritten.","The report is a translation review queue; it is not yet a compiled Godot translation resource."]
    (out/"scan_summary.md").write_text("\n".join(summary)+"\n",encoding="utf-8")
    print("\n".join(summary[:8]))
if __name__=="__main__": main()
