from __future__ import annotations

import json
from pathlib import Path
from statistics import median

from PIL import Image, ImageDraw, ImageFont


SRC_DIR = Path("bin/res")
OUT_DIR = Path("bin/res/workspace-icon-atlas-source")
OUT_DIR.mkdir(parents=True, exist_ok=True)

FAMILIES = ["color", "outline", "solid", "rounded", "duotone", "sharp"]

SRC_LAYOUT = {
    "numbers": {"x": 36, "y": 46, "w": 118, "h": 88, "gap_x": 22, "gap_y": 0, "cols": 10, "count": 10},
    "letters": {"x": 37, "y": 195, "w": 100, "h": 80, "gap_x": 15, "gap_y": 12, "cols": 13, "count": 26},
    "daily": {"x": 36, "y": 428, "w": 82, "h": 74, "gap_x": 14, "gap_y": 13, "cols": 5, "count": 20},
    "development": {"x": 534, "y": 428, "w": 82, "h": 74, "gap_x": 14, "gap_y": 13, "cols": 5, "count": 20},
    "office": {"x": 1050, "y": 428, "w": 82, "h": 74, "gap_x": 14, "gap_y": 13, "cols": 5, "count": 20},
    "windows": {"x": 36, "y": 820, "w": 114, "h": 74, "gap_x": 32, "gap_y": 15, "cols": 10, "count": 20},
}

GROUP_ORDER = ["numbers", "letters", "daily", "development", "office", "windows"]

FAMILY_LAYOUT_OVERRIDES = {
    "duotone": {
        "windows": {"x": 36, "y": 820, "w": 118, "h": 74, "gap_x": 28, "gap_y": 15, "cols": 10, "count": 20},
        "office": {"x": 1050, "y": 428, "w": 86, "h": 74, "gap_x": 10, "gap_y": 13, "cols": 5, "count": 20},
    }
}


def slot_code(group: str, offset: int) -> str:
    if group == "numbers":
        return str(offset)
    if group == "letters":
        return chr(ord("A") + offset)
    prefix = {
        "daily": "D",
        "development": "R",
        "office": "O",
        "windows": "W",
    }[group]
    return f"{prefix}{offset + 1:02d}"


def layout_for(family: str, group: str) -> dict:
    return FAMILY_LAYOUT_OVERRIDES.get(family, {}).get(group, SRC_LAYOUT[group])


def crop_box(family: str, group: str, index: int) -> tuple[int, int, int, int]:
    meta = layout_for(family, group)
    row = index // meta["cols"]
    col = index % meta["cols"]
    left = meta["x"] + col * (meta["w"] + meta["gap_x"])
    top = meta["y"] + row * (meta["h"] + meta["gap_y"])
    return (left, top, left + meta["w"], top + meta["h"])


def sample_bg(tile: Image.Image) -> tuple[int, int, int]:
    w, h = tile.size
    points = [
        tile.getpixel((2, 2))[:3],
        tile.getpixel((w - 3, 2))[:3],
        tile.getpixel((2, h - 3))[:3],
        tile.getpixel((w - 3, h - 3))[:3],
    ]
    return (
        round(median(p[0] for p in points)),
        round(median(p[1] for p in points)),
        round(median(p[2] for p in points)),
    )


def content_bbox(tile: Image.Image, threshold: int = 28) -> tuple[int, int, int, int] | None:
    bg = sample_bg(tile)
    w, h = tile.size
    min_x, min_y = w, h
    max_x, max_y = -1, -1
    for y in range(h):
        for x in range(w):
            r, g, b, *_ = tile.getpixel((x, y))
            diff = abs(r - bg[0]) + abs(g - bg[1]) + abs(b - bg[2])
            if diff >= threshold:
                if x < min_x:
                    min_x = x
                if y < min_y:
                    min_y = y
                if x > max_x:
                    max_x = x
                if y > max_y:
                    max_y = y
    if max_x < min_x or max_y < min_y:
        return None
    return (min_x, min_y, max_x + 1, max_y + 1)


def bbox_to_norm(bbox: tuple[int, int, int, int] | None, size: tuple[int, int]) -> dict | None:
    if bbox is None:
        return None
    w, h = size
    l, t, r, b = bbox
    bw, bh = r - l, b - t
    cx = l + bw / 2
    cy = t + bh / 2
    return {
        "left": l / w,
        "top": t / h,
        "right": r / w,
        "bottom": b / h,
        "width": bw / w,
        "height": bh / h,
        "center_x": cx / w,
        "center_y": cy / h,
    }


def aggregate_group(values: list[dict]) -> dict:
    keys = ["left", "top", "right", "bottom", "width", "height", "center_x", "center_y"]
    return {key: median(v[key] for v in values) for key in keys}


def main():
    summary: dict[str, dict] = {"families": {}, "recommended_by_group": {}}
    grouped_norms: dict[str, list[dict]] = {group: [] for group in GROUP_ORDER}
    preview_dir = OUT_DIR / "previews"
    preview_dir.mkdir(parents=True, exist_ok=True)

    try:
        label_font = ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf", 15)
    except OSError:
        label_font = ImageFont.load_default()

    for family in FAMILIES:
        src = Image.open(SRC_DIR / f"workspace-icons-{family}.png").convert("RGBA")
        preview = src.copy()
        draw = ImageDraw.Draw(preview)
        family_dir = OUT_DIR / family
        family_dir.mkdir(parents=True, exist_ok=True)
        family_meta = []

        for group in GROUP_ORDER:
            for index in range(SRC_LAYOUT[group]["count"]):
                code = slot_code(group, index)
                box = crop_box(family, group, index)
                tile = src.crop(box)
                tile_path = family_dir / f"{code}.png"
                tile.save(tile_path)

                bbox = content_bbox(tile)
                norm = bbox_to_norm(bbox, tile.size)
                if norm:
                    grouped_norms[group].append(norm)

                # source crop box
                draw.rectangle(box, outline=(255, 208, 0, 255), width=2)
                # detected content box inside source box
                if bbox is not None:
                    inner = (box[0] + bbox[0], box[1] + bbox[1], box[0] + bbox[2], box[1] + bbox[3])
                    draw.rectangle(inner, outline=(0, 255, 140, 255), width=2)
                # slot label
                tx, ty = box[0] + 4, box[1] + 4
                label_bg = (0, 0, 0, 160)
                tb = draw.textbbox((tx, ty), code, font=label_font)
                draw.rectangle((tb[0] - 2, tb[1] - 1, tb[2] + 2, tb[3] + 1), fill=label_bg)
                draw.text((tx, ty), code, font=label_font, fill=(255, 255, 255, 255))

                family_meta.append(
                    {
                        "slot": code,
                        "group": group,
                        "source_box": {"left": box[0], "top": box[1], "right": box[2], "bottom": box[3]},
                        "tile_size": {"width": tile.width, "height": tile.height},
                        "content_box": None
                        if bbox is None
                        else {"left": bbox[0], "top": bbox[1], "right": bbox[2], "bottom": bbox[3]},
                        "content_norm": norm,
                    }
                )

        summary["families"][family] = family_meta
        preview.save(preview_dir / f"{family}-boxed.png")

    for group, values in grouped_norms.items():
        summary["recommended_by_group"][group] = aggregate_group(values)

    out_json = OUT_DIR / "metrics.json"
    out_json.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(out_json)


if __name__ == "__main__":
    main()
