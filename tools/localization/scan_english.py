#!/usr/bin/env python3
"""Inventory English-looking quoted strings in Orchestrator source files."""
from __future__ import annotations
import argparse, csv, re
from pathlib import Path
from collections import Counter

EXTS = {".gd",".tscn",".tres",".cfg",".gdextension",".cpp",".cc",".c",".h",".hpp",".hh",".xml",".json",".csv",".po",".pot",".md",".txt",".yaml",".yml",".svg",".html",".css",".js",".gdshader"}
SKIP_DIRS = {".git",".godot","build","bin","obj","extern","thirdparty","third_party","vendor","node_modules","__pycache__"}
QUOTED = re.compile(r'''(?P<q>["'])(?P<value>(?:\\.|(?!\1).)*?)(?P=q)''')
WORD = re.compile(r"[A-Za-z][A-Za-z'-]{1,}")
COMMON_UI = set("""add remove delete edit create open close save load cancel apply reset yes no ok error warning search filter clear select selected enabled disabled name type value default none true false file folder path scene node script input output connect disconnect connection settings editor project resource property properties method signal slot port graph blueprint node tree compile build debug run stop pause resume copy paste cut undo redo dialog welcome loading failed success invalid missing unknown new import export rename duplicate expand collapse refresh update install uninstall enable disable advanced basic general display layout appearance language theme help about version tools inspector filesystem scene viewport history recent""".split())

def unescape(s):
    try: return bytes(s, "utf-8").decode("unicode_escape")
    except Exception: return s

def is_candidate(value):
    s=value.strip()
    if len(s)<2 or not re.search(r"[A-Za-z]",s): return False
    if re.search(r"(?:res|uid|user|https?)://|[A-Za-z]:\\\\|^[/\\\\]",s): return False
    if "\n" in s and len(s)>240: return False
    if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_:.\/-]*",s) and any(x in s for x in "_/.:"): return False
    words=[w.lower() for w in WORD.findall(s)]
    if not words: return False
    return len(words)>=2 or words[0] in COMMON_UI or (len(s)>=12 and " " in s)

def po_quote(s):
    return '"' + s.replace("\\","\\\\").replace('"','\\"').replace("\n","\\n") + '"'

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--root",required=True); ap.add_argument("--out",required=True)
    args=ap.parse_args(); root=Path(args.root).resolve(); out=Path(args.out).resolve(); out.mkdir(parents=True,exist_ok=True)
    rows=[]; seen=set()
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in EXTS: continue
        rel=path.relative_to(root)
        if any(part in SKIP_DIRS for part in rel.parts): continue
        try: content=path.read_text(encoding="utf-8-sig")
        except (UnicodeDecodeError,OSError): continue
        for line_no,line in enumerate(content.splitlines(),1):
            for m in QUOTED.finditer(line):
                value=unescape(m.group("value")).strip()
                if not is_candidate(value): continue
                key=(value,rel.as_posix(),line_no)
                if key in seen: continue
                seen.add(key)
                rows.append({"source_file":rel.as_posix(),"line":line_no,"source_text":value,"translation_zh_CN":"","review_status":"needs_translation"})
    fields=["source_file","line","source_text","translation_zh_CN","review_status"]
    with (out/"english_strings.csv").open("w",newline="",encoding="utf-8-sig") as f:
        w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(rows)
    unique={}
    for r in rows: unique.setdefault(r["source_text"],[]).append(f'{r["source_file"]}:{r["line"]}')
    pot=['msgid ""','msgstr ""','"Content-Type: text/plain; charset=UTF-8\\n"','"Content-Transfer-Encoding: 8bit\\n"','"Language: zh_CN\\n"','']
    for msgid,contexts in sorted(unique.items()):
        pot += ["#: "+" ".join(contexts[:20]),"msgid "+po_quote(msgid),'msgstr ""',""]
    (out/"orchestrator_zh_CN.pot").write_text("\n".join(pot),encoding="utf-8")
    by_ext=Counter(Path(r["source_file"]).suffix.lower() for r in rows)
    summary=["# Orchestrator English-string audit","",f"- Candidate occurrences: **{len(rows)}**",f"- Unique source strings: **{len(unique)}**",f"- Files containing candidates: **{len(set(r['source_file'] for r in rows))}**","","## By file type","","| Extension | Candidate occurrences |","|---|---:|"]
    summary += [f"| `{ext or '(none)'}` | {count} |" for ext,count in sorted(by_ext.items())]
    summary += ["","## Output","","- `english_strings.csv`: source path, line, English candidate, blank Simplified Chinese translation, review status.","- `orchestrator_zh_CN.pot`: gettext template for translation review.","","Audit-only: source files are never modified. The candidate list is a review queue; some entries may be technical text rather than visible UI."]
    (out/"scan_summary.md").write_text("\n".join(summary)+"\n",encoding="utf-8")
    print("\n".join(summary[:8]))
if __name__=="__main__": main()
