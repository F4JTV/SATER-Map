#!/usr/bin/env python3
"""List the translatable strings of the sources, and those the French
catalog lacks.

    python3 scripts/list_tr_strings.py            every string, one per line
    python3 scripts/list_tr_strings.py --missing  only those without French
    python3 scripts/list_tr_strings.py --unused   catalog entries no source uses

A string is the argument of tr("..."), qsTr("...") or SATER_TR("...") in
src/ and qml/ (C++ adjacent literals are joined, as the compiler does).
Strings are printed as C literals, ready to paste into
src/i18n/catalog_fr.inc.  Exits 1 when --missing finds something.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CALL = re.compile(r'\b(?:qsTr|tr|SATER_TR)\s*\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)', re.S)
LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')
ENTRY = re.compile(r'^\{\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\},\s*$')


def strip_comments(text):
    # Line comments only where they cannot be inside a string: good enough
    # for these sources (no "//" inside translatable strings).
    out = []
    for line in text.splitlines():
        in_str = False
        cut = len(line)
        i = 0
        while i < len(line):
            c = line[i]
            if c == '\\' and in_str:
                i += 2
                continue
            if c == '"':
                in_str = not in_str
            elif not in_str and line.startswith("//", i):
                cut = i
                break
            i += 1
        out.append(line[:cut])
    return "\n".join(out)


def source_strings():
    found = {}
    files = sorted(list((ROOT / "src").rglob("*.cpp")) + list((ROOT / "src").rglob("*.h"))
                   + list((ROOT / "qml").glob("*.qml")))
    for path in files:
        text = strip_comments(path.read_text(encoding="utf-8"))
        for m in CALL.finditer(text):
            joined = "".join(LITERAL.findall(m.group(1)))
            found.setdefault(joined, path.relative_to(ROOT).as_posix())
    return found


def catalog():
    entries = {}
    path = ROOT / "src/i18n/catalog_fr.inc"
    for line in path.read_text(encoding="utf-8").splitlines():
        m = ENTRY.match(line.strip())
        if m:
            entries[m.group(1)] = m.group(2)
    return entries


def main():
    strings = source_strings()
    cat = catalog()
    if "--missing" in sys.argv:
        missing = [s for s in strings if s not in cat]
        for s in missing:
            print(f'{{"{s}", ""}},  // {strings[s]}')
        print(f"{len(missing)} missing of {len(strings)}", file=sys.stderr)
        return 1 if missing else 0
    if "--unused" in sys.argv:
        for s in cat:
            if s not in strings:
                print(s)
        return 0
    for s in strings:
        print(s)
    return 0


if __name__ == "__main__":
    sys.exit(main())
