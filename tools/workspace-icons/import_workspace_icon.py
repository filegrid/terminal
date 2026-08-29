from __future__ import annotations

import argparse
from pathlib import Path
from collections import deque

from PIL import Image


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--size", type=int, default=48)
    parser.add_argument("--green-screen", action="store_true")
    args = parser.parse_args()

    src = Path(args.input)
    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)

    img = Image.open(src).convert("RGBA")
    pix = img.load()
    w, h = img.size

    if args.green_screen:
        visited = [[False] * w for _ in range(h)]

        def is_green_bg(x: int, y: int) -> bool:
            r, g, b, a = pix[x, y]
            return a > 0 and g > 220 and r < 60 and b < 60

        q = deque()
        for x in range(w):
            if is_green_bg(x, 0):
                q.append((x, 0))
                visited[0][x] = True
            if is_green_bg(x, h - 1) and not visited[h - 1][x]:
                q.append((x, h - 1))
                visited[h - 1][x] = True
        for y in range(h):
            if is_green_bg(0, y) and not visited[y][0]:
                q.append((0, y))
                visited[y][0] = True
            if is_green_bg(w - 1, y) and not visited[y][w - 1]:
                q.append((w - 1, y))
                visited[y][w - 1] = True

        while q:
            x, y = q.popleft()
            pix[x, y] = (0, 0, 0, 0)
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < w and 0 <= ny < h and not visited[ny][nx] and is_green_bg(nx, ny):
                    visited[ny][nx] = True
                    q.append((nx, ny))

    bbox = img.getbbox()
    if not bbox:
        raise SystemExit("empty image after background removal")
    img = img.crop(bbox)

    scale = min(args.size / img.width, args.size / img.height)
    nw = max(1, round(img.width * scale))
    nh = max(1, round(img.height * scale))
    img = img.resize((nw, nh), Image.LANCZOS)

    tile = Image.new("RGBA", (args.size, args.size), (0, 0, 0, 0))
    ox = (args.size - nw) // 2
    oy = (args.size - nh) // 2
    tile.alpha_composite(img, (ox, oy))
    tile.save(out)
    print(out)


if __name__ == "__main__":
    main()
