from __future__ import annotations

from pathlib import Path

from PIL import Image


BASE_DIR = Path("bin/res/workspace-icon-atlas-generated-square")
ASSETS_DIR = BASE_DIR / "assets"
OUT_DIR = BASE_DIR / "composed"
OUT_DIR.mkdir(parents=True, exist_ok=True)

COLS = 10
ROWS = 12
TILE = 88
GAP = 14
MARGIN_X = 36
MARGIN_Y = 36
WIDTH = MARGIN_X * 2 + COLS * TILE + (COLS - 1) * GAP
HEIGHT = MARGIN_Y * 2 + ROWS * TILE + (ROWS - 1) * GAP
BG = (7, 17, 31, 255)

START_ROWS = {
    "numbers": 0,
    "letters": 1,
    "daily": 4,
    "development": 6,
    "office": 8,
    "windows": 10,
}

COUNTS = {
    "numbers": 10,
    "letters": 26,
    "daily": 20,
    "development": 20,
    "office": 20,
    "windows": 20,
}

PREFIX = {
    "numbers": "",
    "letters": "",
    "daily": "D",
    "development": "R",
    "office": "O",
    "windows": "W",
}


def codes_for(group: str) -> list[str]:
    if group == "numbers":
        return [str(i) for i in range(10)]
    if group == "letters":
        return [chr(ord("A") + i) for i in range(26)]
    p = PREFIX[group]
    return [f"{p}{i + 1:02d}" for i in range(COUNTS[group])]


def dst_xy(group: str, index: int) -> tuple[int, int]:
    row = START_ROWS[group] + index // COLS
    col = index % COLS
    x = MARGIN_X + col * (TILE + GAP)
    y = MARGIN_Y + row * (TILE + GAP)
    return x, y


def fit_icon(img: Image.Image, inner: int = 66) -> Image.Image:
    if img.mode != "RGBA":
        img = img.convert("RGBA")
    bbox = img.getbbox()
    if bbox:
        img = img.crop(bbox)
    scale = min(inner / img.width, inner / img.height)
    w = max(1, round(img.width * scale))
    h = max(1, round(img.height * scale))
    img = img.resize((w, h), Image.LANCZOS)
    tile = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    px = (TILE - w) // 2
    py = (TILE - h) // 2
    tile.alpha_composite(img, (px, py))
    return tile


def compose_family(family: str) -> Path:
    canvas = Image.new("RGBA", (WIDTH, HEIGHT), BG)
    family_dir = ASSETS_DIR / family
    for group in START_ROWS:
        for index, code in enumerate(codes_for(group)):
            src = family_dir / f"{code}.png"
            if not src.exists():
                continue
            with Image.open(src) as im:
                tile = fit_icon(im)
            x, y = dst_xy(group, index)
            canvas.alpha_composite(tile, (x, y))
    out = OUT_DIR / f"{family}.png"
    canvas.save(out)
    return out


def main():
    for family in ["color", "outline", "solid", "rounded", "duotone", "sharp"]:
        out = compose_family(family)
        print(out)


if __name__ == "__main__":
    main()
