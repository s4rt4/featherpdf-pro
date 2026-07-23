# Feather PDF — Nautilus (GNOME Files) context-menu extension.
#
# Adds a "Feather PDF" submenu to the right-click menu for selected PDFs, with
# one-click headless actions (Compress, Remove Hidden Info, Extract Text). Each
# entry shells out to the shared `feather-pdf-action` helper, which writes the
# result beside the source and reports via a desktop notification.
#
# Requires nautilus-python. Installed into share/nautilus-python/extensions.

from urllib.parse import unquote, urlparse

import gi

# The Nautilus extension API version tracks the GNOME release, so don't hard-pin
# it — take the newest typelib the system actually ships.
for _ver in ("4.1", "4.0", "3.0"):
    try:
        gi.require_version("Nautilus", _ver)
        break
    except ValueError:
        continue

from gi.repository import GObject, Nautilus  # noqa: E402

PDF_MIME = "application/pdf"

# (label, helper action, tooltip) for each submenu entry.
ACTIONS = (
    ("Compress", "compress", "Shrink the PDF (writes …-compressed.pdf beside it)"),
    ("Remove Hidden Info", "sanitize",
     "Strip metadata, attachments, and scripts (writes …-clean.pdf)"),
    ("Extract Text", "to-text", "Save the document text as a .txt file"),
)


def _local_pdf_paths(files):
    """Return local filesystem paths if every selected file is a local PDF,
    else None (so the menu stays hidden for mixed or remote selections)."""
    paths = []
    for f in files:
        if f.get_uri_scheme() != "file" or f.get_mime_type() != PDF_MIME:
            return None
        paths.append(unquote(urlparse(f.get_uri()).path))
    return paths


class FeatherPdfMenuProvider(GObject.GObject, Nautilus.MenuProvider):
    def _run(self, _menu_item, action, paths):
        # Detached: the file manager shouldn't block on the conversion.
        import subprocess

        subprocess.Popen(["feather-pdf-action", action, *paths])

    def get_file_items(self, files):
        paths = _local_pdf_paths(files) if files else None
        if not paths:
            return []
        top = Nautilus.MenuItem(
            name="FeatherPdf::Menu", label="Feather PDF", tip="PDF actions")
        submenu = Nautilus.Menu()
        top.set_submenu(submenu)
        for label, action, tip in ACTIONS:
            item = Nautilus.MenuItem(
                name=f"FeatherPdf::{action}", label=label, tip=tip)
            item.connect("activate", self._run, action, paths)
            submenu.append_item(item)
        return [top]
