#!/usr/bin/env python3
"""Extract one release's notes from a release-please CHANGELOG.md.

release-please writes each release as a level-2 heading followed by
"### <Category>" sections of "* <change>" bullets, where every bullet ends with
markdown links to the PR/issue and the commit, e.g.:

    ## [1.6.0](https://.../compare/v1.5.0...v1.6.0) (2026-07-02)

    ### Features

    * **rpm:** build RPMs for aarch64 too ([#62](url)) ([638724f](url))

This script pulls the section for one version and re-emits it either as plain
bullets for debian/changelog (`deb`) or as an AppStream <release> element with a
<description>, injected into one or more metainfo files (`appstream-inject`).
The two package "changelog" surfaces (Launchpad / `apt changelog` on the Debian
side, and the "What's New" pane in KDE Discover / GNOME Software on the
AppStream side) then both show the real changes, sourced from one file.
"""

from __future__ import annotations

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

# A markdown link "[text](url)" — used both to unwrap links and to strip the
# trailing "([#62](url)) ([hash](url))" reference tail release-please appends.
LINK = re.compile(r"\[([^\]]+)\]\([^)]*\)")
TRAILING_REFS = re.compile(r"(?:\s*\(\[[^\]]+\]\([^)]*\)\))+\s*$")


def _norm(version: str) -> str:
    return version.lstrip("v").strip()


def parse_section(changelog: str, version: str):
    """Return (date, [(category, [bullet, ...]), ...]) for one version.

    date may be None. Raises SystemExit if the version heading is not found.
    """
    want = _norm(version)
    lines = changelog.splitlines()

    # Locate the "## [x.y.z] ... (date)" (or "## x.y.z ...") heading.
    heading = re.compile(
        r"^##\s+\[?v?" + re.escape(want) + r"(?:\])?(?:[\s(]|$)"
    )
    date_re = re.compile(r"\((\d{4}-\d{2}-\d{2})\)")
    start = None
    date = None
    for i, line in enumerate(lines):
        if heading.match(line):
            start = i
            m = date_re.search(line)
            if m:
                date = m.group(1)
            break
    if start is None:
        sys.exit(f"changelog-release-notes: version {want} not found in changelog")

    categories: list[tuple[str, list[str]]] = []
    current: list[str] | None = None
    for line in lines[start + 1 :]:
        if line.startswith("## "):
            break  # next release
        m = re.match(r"^#{3,}\s+(.*)$", line)
        if m:
            current = []
            categories.append((m.group(1).strip(), current))
            continue
        m = re.match(r"^\s*[*-]\s+(.*)$", line)
        if m and current is not None:
            current.append(_clean(m.group(1)))
    # Drop empty categories (e.g. a heading with no bullets).
    categories = [(c, b) for c, b in categories if b]
    return date, categories


def _clean(text: str) -> str:
    text = TRAILING_REFS.sub("", text)          # drop "([#62](url)) ([hash](url))"
    text = text.replace("**", "")               # "**rpm:**" -> "rpm:"
    text = LINK.sub(r"\1", text)                # any leftover [text](url) -> text
    return text.strip()


def emit_deb(categories) -> list[str]:
    """Flat bullet list for `dch`; category names are folded into the bullets."""
    bullets: list[str] = []
    for category, items in categories:
        for item in items:
            bullets.append(item)
    return bullets


def build_description(categories) -> ET.Element:
    desc = ET.Element("description")
    single = len(categories) == 1
    for category, items in categories:
        if not single:
            p = ET.SubElement(desc, "p")
            p.text = category
        ul = ET.SubElement(desc, "ul")
        for item in items:
            li = ET.SubElement(ul, "li")
            li.text = item
    return desc


def inject(metainfo_path: Path, version: str, date: str | None, categories) -> bool:
    """Insert a <release> with <description> as the newest child of <releases>.

    Idempotent: if the version is already present, replace its description so
    re-runs stay clean. Returns True if the file changed.
    """
    want = _norm(version)
    ET.register_namespace("", "")
    tree = ET.parse(metainfo_path)
    root = tree.getroot()
    releases = root.find("releases")
    if releases is None:
        releases = ET.SubElement(root, "releases")

    existing = None
    for rel in releases.findall("release"):
        if rel.get("version") == want:
            existing = rel
            break

    rel = existing if existing is not None else ET.Element("release")
    rel.set("version", want)
    if date:
        rel.set("date", date)
    # Refresh the description in place.
    for child in list(rel):
        rel.remove(child)
    if categories:
        rel.append(build_description(categories))

    if existing is None:
        releases.insert(0, rel)

    ET.indent(tree, space="  ")
    tree.write(metainfo_path, encoding="UTF-8", xml_declaration=True)
    # ElementTree omits the trailing newline; keep files newline-terminated.
    with open(metainfo_path, "a", encoding="UTF-8") as fh:
        fh.write("\n")
    return True


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p_deb = sub.add_parser("deb", help="print cleaned bullets, one per line")
    p_deb.add_argument("version")
    p_deb.add_argument("--changelog", default="CHANGELOG.md")

    p_as = sub.add_parser("appstream-inject", help="add a <release> to metainfo files")
    p_as.add_argument("version")
    p_as.add_argument("metainfo", nargs="+")
    p_as.add_argument("--changelog", default="CHANGELOG.md")
    p_as.add_argument("--date", help="override the release date (default: from changelog)")

    args = ap.parse_args()
    changelog = Path(args.changelog).read_text(encoding="UTF-8")
    date, categories = parse_section(changelog, args.version)

    if args.cmd == "deb":
        bullets = emit_deb(categories)
        if not bullets:
            sys.exit("changelog-release-notes: no bullets found for this version")
        print("\n".join(bullets))
    elif args.cmd == "appstream-inject":
        for path in args.metainfo:
            inject(Path(path), args.version, args.date or date, categories)


if __name__ == "__main__":
    main()
