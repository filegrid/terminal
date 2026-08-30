from pathlib import Path

from PIL import Image, ImageDraw


BASE_DIR = Path("bin/res/workspace-icon-atlas-generated-square")
BASE_DIR.mkdir(parents=True, exist_ok=True)

COLS = 10
ROWS = 12
ICON = 48

BG = (10, 18, 30, 255)
CELL_A = (18, 30, 46, 255)
CELL_B = (22, 36, 54, 255)
GRID = (42, 68, 98, 255)


def make_checker(cols: int, rows: int) -> Image.Image:
    width = cols * ICON
    height = rows * ICON
    image = Image.new("RGBA", (width, height), BG)
    draw = ImageDraw.Draw(image)
    for row in range(rows):
        for col in range(cols):
            fill = CELL_A if (row + col) % 2 == 0 else CELL_B
            x0 = col * ICON
            y0 = row * ICON
            x1 = x0 + ICON - 1
            y1 = y0 + ICON - 1
            draw.rectangle((x0, y0, x1, y1), fill=fill)
    for x in range(0, width + 1, ICON):
        draw.line((x, 0, x, height), fill=GRID, width=1)
    for y in range(0, height + 1, ICON):
        draw.line((0, y, width, y), fill=GRID, width=1)
    return image


def main():
    atlas = make_checker(COLS, ROWS)
    atlas.save(BASE_DIR / "template-48.png")

    numbers = make_checker(10, 1)
    numbers.save(BASE_DIR / "template-48-numbers.png")

    print(BASE_DIR / "template-48.png")
    print(BASE_DIR / "template-48-numbers.png")


if __name__ == "__main__":
    main()
