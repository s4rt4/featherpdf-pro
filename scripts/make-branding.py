"""Feather PDF Pro - regenerate the Windows branding artwork.

Everything here is derived from resources/icons/feather-logo.svg, so the app
icon and the installer wizard never drift from the logo. Run it after the logo
changes and commit the result:

    python scripts/make-branding.py

Produces:
    resources/windows/feather.ico        icon compiled into feather-pdf-pro.exe
                                         (and used as the installer's own icon)
    resources/windows/pdf-document.ico   Explorer's icon for a PDF with no cover
    resources/windows/pdf-badge.ico      badge stamped on a rendered PDF cover
    packaging/windows/wizard-large*.bmp  installer welcome/finish panel
    packaging/windows/wizard-small*.bmp  installer header logo

Needs Pillow and Google Chrome. Chrome does the SVG rasterizing because
Pillow cannot read SVG and the alternatives (Inkscape, ImageMagick, cairosvg)
are not part of this project's toolchain. Inno Setup cannot read alpha, so the
wizard images are flattened onto their background here.
"""

import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFont

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICON_SRC = os.path.join(REPO, "resources", "icons")
LOGO_SVG = os.path.join(ICON_SRC, "feather-logo.svg")

# <source SVG> -> <icon written into resources/windows>
ICONS = {
    "feather-logo.svg": "feather.ico",
    "pdf-document.svg": "pdf-document.ico",
    "pdf-badge.svg": "pdf-badge.ico",
}

# Brand colours, from the logo and the app's dark palette.
TEAL = (17, 124, 111)
PANEL_BG = (13, 59, 53)
PANEL_TEXT = (255, 255, 255)
PANEL_SUBTEXT = (150, 196, 189)

TITLE = "Feather PDF Pro"
TAGLINE = ["Light on the system,", "full-featured on PDF."]

# Inno Setup picks the closest image to the user's DPI scaling out of these.
WIZARD_LARGE_SIZES = [(164, 314), (246, 471), (328, 628)]
WIZARD_SMALL_SIZES = [(55, 55), (83, 83), (110, 110)]
ICON_SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]

CHROME_CANDIDATES = [
    os.path.expandvars(r"%ProgramFiles%\Google\Chrome\Application\chrome.exe"),
    os.path.expandvars(r"%ProgramFiles(x86)%\Google\Chrome\Application\chrome.exe"),
    os.path.expandvars(r"%LOCALAPPDATA%\Google\Chrome\Application\chrome.exe"),
]


def find_chrome():
    for path in CHROME_CANDIDATES:
        if os.path.isfile(path):
            return path
    sys.exit("Google Chrome not found - it is needed to rasterize the SVG.")


def render_svg(path, size):
    """Rasterize an SVG to a transparent RGBA image of size x size."""
    with open(path, encoding="utf-8") as handle:
        svg = handle.read()
    # The SVG is a bare 512x512 viewBox; stretch it over a margin-free page so
    # the screenshot is exactly the artwork.
    html = (
        "<style>html,body{margin:0;padding:0;background:transparent}"
        "svg{display:block;width:100vw;height:100vh}</style>" + svg
    )
    with tempfile.TemporaryDirectory() as tmp:
        page = os.path.join(tmp, "logo.html")
        shot = os.path.join(tmp, "logo.png")
        with open(page, "w", encoding="utf-8") as handle:
            handle.write(html)
        subprocess.run(
            [
                find_chrome(),
                "--headless",
                "--disable-gpu",
                "--hide-scrollbars",
                "--default-background-color=00000000",
                "--screenshot=" + shot,
                "--window-size=%d,%d" % (size, size),
                "file:///" + page.replace("\\", "/"),
            ],
            check=True,
            capture_output=True,
        )
        return Image.open(shot).convert("RGBA")


def font(name, size):
    return ImageFont.truetype(os.path.join(os.environ["SystemRoot"], "Fonts", name), size)


def centred(draw, y, text, fnt, fill, width):
    left, top, right, bottom = draw.textbbox((0, 0), text, font=fnt)
    draw.text(((width - (right - left)) / 2 - left, y - top), text, font=fnt, fill=fill)
    return bottom - top


def wizard_large(logo, size):
    """The tall panel on the welcome and finish pages: mark, name, tagline."""
    width, height = size
    scale = width / 164.0
    panel = Image.new("RGB", size, PANEL_BG)
    mark = round(86 * scale)
    panel.paste(
        logo.resize((mark, mark), Image.LANCZOS),
        ((width - mark) // 2, round(52 * scale)),
        logo.resize((mark, mark), Image.LANCZOS),
    )
    draw = ImageDraw.Draw(panel)
    y = round(168 * scale)
    y += centred(draw, y, TITLE, font("segoeuib.ttf", round(13 * scale)), PANEL_TEXT, width)
    y += round(11 * scale)
    small = font("segoeui.ttf", round(9.5 * scale))
    for line in TAGLINE:
        y += centred(draw, y, line, small, PANEL_SUBTEXT, width) + round(4 * scale)
    return panel


def wizard_small(logo, size):
    """The header logo, drawn on the wizard's white header strip."""
    panel = Image.new("RGB", size, (255, 255, 255))
    mark = logo.resize(size, Image.LANCZOS)
    panel.paste(mark, (0, 0), mark)
    return panel


def main():
    icon_dir = os.path.join(REPO, "resources", "windows")
    os.makedirs(icon_dir, exist_ok=True)
    for source, name in ICONS.items():
        out = os.path.join(icon_dir, name)
        render_svg(os.path.join(ICON_SRC, source), 1024).save(
            out, sizes=[(s, s) for s in ICON_SIZES]
        )
        print("wrote", out)

    logo = render_svg(LOGO_SVG, 1024)
    packaging = os.path.join(REPO, "packaging", "windows")
    for width, height in WIZARD_LARGE_SIZES:
        out = os.path.join(packaging, "wizard-large-%d.bmp" % width)
        wizard_large(logo, (width, height)).save(out)
        print("wrote", out)
    for width, height in WIZARD_SMALL_SIZES:
        out = os.path.join(packaging, "wizard-small-%d.bmp" % width)
        wizard_small(logo, (width, height)).save(out)
        print("wrote", out)


if __name__ == "__main__":
    main()
