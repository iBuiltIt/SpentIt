#!/usr/bin/env python3
"""Prepare the offline Help bundle and reject links that leave it.

Authored topics are Markdown files with frontmatter. This writes one manifest
and copies bundled images. It does not open a network connection or launch a
viewer. The running viewer applies the same allowlist again.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from pathlib import Path


class PrepareError(Exception):
    pass


IMAGE_RE = re.compile(r"!\[([^\]]*)\]\(([^)\s]+)\)")
LINK_RE = re.compile(r"(?<!!)\[([^\]]*)\]\(([^)\s]+)\)")
HEADING_RE = re.compile(r"^(#{1,6})\s+(.+?)\s*$")
ID_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
TOKEN_RE = re.compile(r"\x00IMG(\d+)\x00")


def fail(message: str) -> None:
    raise PrepareError(message)


def parse_frontmatter(text: str, path: Path) -> tuple[dict[str, str], str]:
    if not text.startswith("---\n"):
        fail(f"{path}: a topic must start with frontmatter")
    end = text.find("\n---\n", 4)
    if end < 0:
        fail(f"{path}: frontmatter is not closed")
    raw = text[4:end]
    body = text[end + 5 :]
    if body.startswith("\n"):
        body = body[1:]
    fields: dict[str, str] = {}
    for line in raw.splitlines():
        if not line.strip():
            continue
        if ":" not in line:
            fail(f"{path}: frontmatter line {line!r} needs a colon")
        key, value = line.split(":", 1)
        key = key.strip()
        if key in fields:
            fail(f"{path}: duplicate frontmatter field {key}")
        fields[key] = value.strip()
    return fields, body


def slug(title: str) -> str:
    letters = []
    previous_dash = False
    for character in title.lower():
        if character.isalnum():
            letters.append(character)
            previous_dash = False
        elif not previous_dash:
            letters.append("-")
            previous_dash = True
    return "".join(letters).strip("-")


def heading_title(text: str) -> tuple[str, str | None]:
    match = re.fullmatch(r"(.+?)\s*\{#([a-z0-9]+(?:-[a-z0-9]+)*)\}\s*", text)
    if match:
        return match.group(1).strip(), match.group(2)
    return text.strip(), None


def parse_sections(body: str, path: Path) -> tuple[str, list[dict[str, str]]]:
    sections: list[dict[str, str]] = []
    seen: set[str] = set()
    lines = []
    for line in body.splitlines():
        match = HEADING_RE.match(line)
        if not match:
            lines.append(line)
            continue
        title, explicit = heading_title(match.group(2))
        section_id = explicit or slug(title)
        if not section_id or not ID_RE.fullmatch(section_id):
            fail(f"{path}: heading {title!r} needs a plain section id")
        if section_id in seen:
            fail(f"{path}: duplicate section id {section_id}")
        seen.add(section_id)
        sections.append({"id": section_id, "title": title})
        lines.append(f"{match.group(1)} {title}")
    return "\n".join(lines).strip() + "\n", sections


def reject_target(raw: str, source: Path) -> None:
    lowered = raw.lower()
    if "://" in raw or lowered.startswith(("http:", "https:", "file:", "javascript:", "data:", "mailto:", "qrc:", "help:")):
        fail(f"{source}: link {raw} is not a bundled topic or image")
    if ":" in raw.split("/")[0]:
        fail(f"{source}: link {raw} is not a bundled topic or image")
    if raw.startswith(("/", "\\", "~")) or "\\" in raw:
        fail(f"{source}: link {raw} is not a bundled topic or image")
    parts = raw.split("/")
    if any(part in ("", ".") for part in parts):
        fail(f"{source}: link {raw} is not a bundled topic or image")


def resolve_inside(source: Path, raw: str, root: Path) -> Path:
    reject_target(raw, source)
    target = (source.parent / raw).resolve()
    root_resolved = root.resolve()
    if not target.is_relative_to(root_resolved):
        fail(f"{source}: link {raw} leaves the Help bundle")
    return target


def plain_text(markdown: str) -> str:
    without_images = IMAGE_RE.sub(lambda match: match.group(1), markdown)
    without_links = LINK_RE.sub(lambda match: match.group(1), without_images)
    return re.sub(r"\s+", " ", without_links).strip()


def caption_follows(body: str, image_end: int, source: Path) -> None:
    rest = body[image_end:]
    line = ""
    for candidate in rest.splitlines():
        if candidate.strip():
            line = candidate.strip()
            break
    if not (line.startswith("*") and line.endswith("*") and len(line) > 2):
        fail(f"{source}: an image needs an italic caption on the next line")


def validate_context_sources(topics: list[dict], sources: list[Path]) -> None:
    """Every literal application Help target must resolve before bundling."""
    by_id = {topic["id"]: topic for topic in topics}
    for source in sources:
        text = source.read_text(encoding="utf-8")
        for target in re.findall(r"""["'](help:[^"']*)["']""", text):
            match = re.fullmatch(r"help:([a-z0-9-]+)(?:#([a-z0-9-]+))?", target)
            if not match or match[1] not in by_id:
                fail(f"{source}: context target {target} is not a bundled topic")
            if match[2] and not any(section["id"] == match[2] for section in by_id[match[1]]["sections"]):
                fail(f"{source}: context target {target} is not a bundled section")


def build(manual: Path, output: Path, context_sources: list[Path] | None = None) -> None:
    if not manual.is_dir():
        fail(f"{manual} is not a Help source directory")
    topics = []
    for path in sorted(manual.rglob("*.md")):
        text = path.read_text(encoding="utf-8")
        if not text.startswith("---\n"):
            continue
        fields, body = parse_frontmatter(text, path)
        for key in ("id", "title", "order"):
            if key not in fields or not fields[key]:
                fail(f"{path}: missing {key}")
        if not ID_RE.fullmatch(fields["id"]):
            fail(f"{path}: id {fields['id']!r} must be lowercase words")
        try:
            order = int(fields["order"])
        except ValueError:
            fail(f"{path}: order must be a whole number")
        rewritten, sections = parse_sections(body, path)
        keywords = [part.strip() for part in fields.get("keywords", "").split(",") if part.strip()]
        topics.append({
            "path": path.resolve(),
            "id": fields["id"],
            "title": fields["title"],
            "order": order,
            "keywords": keywords,
            "body": rewritten,
            "sections": sections,
        })
    if not topics:
        fail(f"{manual}: no Help topics")
    ids = [topic["id"] for topic in topics]
    if len(ids) != len(set(ids)):
        fail("duplicate Help topic id")
    validate_context_sources(topics, context_sources or [])
    by_path = {topic["path"]: topic for topic in topics}
    images: dict[str, dict[str, str]] = {}

    def take_image(source: Path, alt: str, raw: str) -> str:
        if not alt.strip():
            fail(f"{source}: an image needs alternative text")
        target = resolve_inside(source, raw, manual)
        if target.suffix.lower() != ".png" or not target.is_file():
            fail(f"{source}: image {raw} is not a bundled PNG")
        image_id = target.name
        previous = images.get(image_id)
        if previous and previous["source"] != str(target):
            fail(f"{source}: image name {image_id} is used by two files")
        images[image_id] = {"id": image_id, "alt": alt.strip(), "source": str(target)}
        return image_id

    rendered = []
    for topic in topics:
        body = topic["body"]
        stored = []

        def replace_image(match: re.Match[str], source: Path = topic["path"], original: str = body) -> str:
            caption_follows(original, match.end(), source)
            image_id = take_image(source, match.group(1), match.group(2))
            stored.append(
                f"[![{match.group(1).strip()}](qrc:/spentit/help/images/{image_id})]"
                f"(help-image:{image_id})"
            )
            return f"\x00IMG{len(stored) - 1}\x00"

        without_images = IMAGE_RE.sub(replace_image, body)

        def replace_link(match: re.Match[str], source: Path = topic["path"]) -> str:
            raw = match.group(2)
            fragment = ""
            path_part = raw
            if "#" in raw:
                path_part, fragment = raw.split("#", 1)
                if not fragment or "#" in fragment:
                    fail(f"{source}: link {raw} has no usable section")
            if path_part == "":
                target = topic
            else:
                resolved = resolve_inside(source, path_part, manual)
                target = by_path.get(resolved)
                if target is None:
                    fail(f"{source}: link {raw} is not a bundled topic or image")
            if fragment and all(section["id"] != fragment for section in target["sections"]):
                fail(f"{source}: link {raw} is not a bundled section")
            href = "help:" + target["id"] + (f"#{fragment}" if fragment else "")
            return f"[{match.group(1)}]({href})"

        linked = LINK_RE.sub(replace_link, without_images)

        def restore(match: re.Match[str]) -> str:
            return stored[int(match.group(1))]

        topic["markdown"] = TOKEN_RE.sub(restore, linked).strip() + "\n"
        topic["plain"] = plain_text(topic["markdown"])
        rendered.append(topic)

    if output.exists():
        shutil.rmtree(output)
    image_dir = output / "images"
    image_dir.mkdir(parents=True)
    for image in images.values():
        shutil.copyfile(image["source"], image_dir / image["id"])
    sample = all(topic["id"].startswith("sample-") for topic in rendered)
    manifest = {
        "sample": sample,
        "banner": "This is sample content for checking Help. It is not the SpentIt manual." if sample else "",
        "topics": [
            {
                "id": topic["id"],
                "title": topic["title"],
                "order": topic["order"],
                "keywords": topic["keywords"],
                "sections": topic["sections"],
                "markdown": topic["markdown"],
                "plain": topic["plain"],
            }
            for topic in sorted(rendered, key=lambda item: (item["order"], item["title"], item["id"]))
        ],
        "images": [
            {"id": image["id"], "alt": image["alt"]}
            for image in sorted(images.values(), key=lambda item: item["id"])
        ],
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Prepare the SpentIt Help bundle")
    parser.add_argument("--manual", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--context-source", action="append", default=[], type=Path)
    args = parser.parse_args(argv)
    try:
        build(args.manual, args.output, args.context_source)
    except PrepareError as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
