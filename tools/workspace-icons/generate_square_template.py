from pathlib import Path

from PIL import Image, ImageDraw


OUT = Path("bin/res/workspace-icon-atlas-generated-square/template.png")
OUT.parent.mkdir(parents=True, exist_ok=True)

COLS = 10
ROWS = 12
ICON = 48

WIDTH = COLS * ICON
HEIGHT = ROWS * ICON

BG = (10, 18, 30, 255)
CELL_A = (18, 30, 46, 255)
CELL_B = (22, 36, 54, 255)
GRID = (42, 68, 98, 255)


def main():
    image = Image.new("RGBA", (WIDTH, HEIGHT), BG)
    draw = ImageDraw.Draw(image)

    for row in range(ROWS):
        for col in range(COLS):
            fill = CELL_A if (row + col) % 2 == 0 else CELL_B
            x0 = col * ICON
            y0 = row * ICON
            x1 = x0 + ICON - 1
            y1 = y0 + ICON - 1
            draw.rectangle((x0, y0, x1, y1), fill=fill)

    for x in range(0, WIDTH + 1, ICON):
        draw.line((x, 0, x, HEIGHT), fill=GRID, width=1)
    for y in range(0, HEIGHT + 1, ICON):
        draw.line((0, y, WIDTH, y), fill=GRID, width=1)

    image.save(OUT)
    print(OUT)


if __name__ == "__main__":
    main()
