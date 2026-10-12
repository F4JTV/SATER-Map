#!/usr/bin/env python3
"""Write the application icon at every size the program, the Linux menu,
Windows and Android need, from the emblem assets/logo.svg.

    python3 scripts/make_icons.py              # renders assets/logo.svg
    python3 scripts/make_icons.py picture.png  # or starts from a square PNG

The emblem's lettering follows arcs (SVG textPath), which Qt's SVG module
and ImageMagick do not draw: the SVG is rendered by Chromium through
Playwright (pip install playwright; playwright install chromium), at
1024 px, with a transparent background outside the disc.

Writes assets/logo.png (1024 px: window icon, About box, PDF report),
assets/logo.ico (16 to 256 px), android/res/drawable-*/icon.png, the splash
logos android/res/drawable/splash_logo.png (Qt's splash screen) and
android/res/drawable-nodpi/splash_emblem.png (Android 12's), and
packaging/icons/<size>/sater-map.png.
"""
import sys
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
S = 1024


def render_svg(svg_path, size):
    from playwright.sync_api import sync_playwright

    svg = svg_path.read_text(encoding="utf-8")
    html = ("<!doctype html><html><head><style>html,body{margin:0;padding:0;background:transparent}"
            f"svg{{display:block;width:{size}px;height:{size}px}}</style></head><body>{svg}</body></html>")
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "logo.png"
        with sync_playwright() as p:
            browser = p.chromium.launch()
            page = browser.new_page(viewport={"width": size, "height": size})
            page.set_content(html)
            page.wait_for_timeout(300)      # web fonts, if any
            page.screenshot(path=str(out), omit_background=True,
                            clip={"x": 0, "y": 0, "width": size, "height": size})
            browser.close()
        return Image.open(out).convert("RGBA").copy()


def save(img, path, px):
    path.parent.mkdir(parents=True, exist_ok=True)
    img.resize((px, px), Image.LANCZOS).save(path, optimize=True)


def main():
    if len(sys.argv) > 1:
        logo = Image.open(sys.argv[1]).convert("RGBA").resize((S, S), Image.LANCZOS)
    else:
        logo = render_svg(ROOT / "assets/logo.svg", S)
    save(logo, ROOT / "assets/logo.png", S)
    logo.save(ROOT / "assets/logo.ico", sizes=[(n, n) for n in (16, 24, 32, 48, 64, 128, 256)])
    for folder, px in (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)):
        save(logo, ROOT / f"android/res/drawable-{folder}/icon.png", px)
    save(logo, ROOT / "android/res/drawable/splash_logo.png", 320)
    # Android 12 and later: the system's splash screen, at full resolution
    # (android/res/drawable/splash_icon.xml insets it into the circle).
    save(logo, ROOT / "android/res/drawable-nodpi/splash_emblem.png", 768)
    for px in (48, 64, 128, 256):
        save(logo, ROOT / f"packaging/icons/{px}x{px}/sater-map.png", px)


if __name__ == "__main__":
    main()
