from pathlib import Path

from PIL import Image


OUT_DIR = Path("bin/res/workspace-icon-atlas-preview-fixed")
OUT_DIR.mkdir(parents=True, exist_ok=True)

W, H = 1600, 1280
BG = (12, 18, 28, 255)

MARGIN_X = 40
MARGIN_Y = 40
CELL_W = 118
CELL_H = 88
GAP = 10
COLS = 10
ROWS = 12

START_ROWS = {
    "numbers": 0,
    "letters": 1,
    "daily": 4,
    "development": 6,
    "office": 8,
    "windows": 10,
}

SRC_LAYOUT = {
    "numbers": {"x": 36, "y": 46, "w": 118, "h": 88, "gap_x": 22, "gap_y": 0, "cols": 10, "count": 10},
    "letters": {"x": 37, "y": 195, "w": 100, "h": 80, "gap_x": 15, "gap_y": 12, "cols": 13, "count": 26},
    "daily": {"x": 36, "y": 428, "w": 82, "h": 74, "gap_x": 14, "gap_y": 13, "cols": 5, "count": 20},
    "development": {"x": 534, "y": 428, "w": 82, "h": 74, "gap_x": 14, "gap_y": 13, "cols": 5, "count": 20},
    "office": {"x": 1050, "y": 428, "w": 82, "h": 74, "gap_x": 14, "gap_y": 13, "cols": 5, "count": 20},
    "windows": {"x": 36, "y": 820, "w": 114, "h": 74, "gap_x": 32, "gap_y": 15, "cols": 10, "count": 20},
}

GROUP_ORDER = ["numbers", "letters", "daily", "development", "office", "windows"]


def dst_cell_box(row: int, col: int):
    x = MARGIN_X + col * (CELL_W + GAP)
    y = MARGIN_Y + row * (CELL_H + GAP)
    return (x, y, x + CELL_W, y + CELL_H)


def grid_position(group: str, offset: int):
    start_row = START_ROWS[group]
    row = start_row + (offset // COLS)
    col = offset % COLS
    return row, col


def extract_group_tiles(src: Image.Image, group: str):
    meta = SRC_LAYOUT[group]
    tiles = []
    for idx in range(meta["count"]):
        row = idx // meta["cols"]
        col = idx % meta["cols"]
        left = meta["x"] + col * (meta["w"] + meta["gap_x"])
        top = meta["y"] + row * (meta["h"] + meta["gap_y"])
        tile = src.crop((left, top, left + meta["w"], top + meta["h"]))
        tiles.append(tile)
    return tiles


def make_family(name: str):
    src = Image.open(Path("bin/res") / f"workspace-icons-{name}.png").convert("RGBA")
    image = Image.new("RGBA", (W, H), BG)

    for group in GROUP_ORDER:
        tiles = extract_group_tiles(src, group)
        for idx, tile in enumerate(tiles):
            row, col = grid_position(group, idx)
            box = dst_cell_box(row, col)
            target_w = tile.width
            target_h = tile.height
            if target_w > CELL_W or target_h > CELL_H:
                scale = min(CELL_W / tile.width, CELL_H / tile.height)
                target_w = max(1, round(tile.width * scale))
                target_h = max(1, round(tile.height * scale))
                tile = tile.resize((target_w, target_h), Image.LANCZOS)
            fitted = tile
            paste_x = box[0] + (CELL_W - target_w) // 2
            paste_y = box[1] + (CELL_H - target_h) // 2
            image.alpha_composite(fitted, (paste_x, paste_y))

    image.save(OUT_DIR / f"workspace-icons-{name}.png")


for family_name in ["color", "outline", "solid", "rounded", "duotone", "sharp"]:
    make_family(family_name)
