# Feather PDF

**Light on the system, full-featured on PDF.**

Feather PDF is an open-source, native PDF application for Linux, aiming at
Adobe Acrobat–class capability without a web view. It wears Acrobat's *bones* —
a tabbed workspace, a per-document command toolbar, a contextual Tools pane —
with a quieter, modern GNOME *skin*.

> Status: early development (milestone **M0** — foundation & viewer). The core
> viewer is usable; editing milestones are on the roadmap below.

## Philosophy

Orchestrate mature libraries behind one clean façade; don't reinvent a PDF
engine.

- **Render (pixels):** PDFium, via Qt's `QtPdf`
- **Semantic layer (text, search, forms, annotations):** Poppler-Qt6
- **Lossless structure (merge, split, rotate, encrypt, metadata):** QPDF
- OCR with Tesseract · conversion with LibreOffice · shaping with HarfBuzz /
  FreeType · signing with OpenSSL

Feather deliberately avoids in-process AGPL libraries (MuPDF, Ghostscript) to
stay GPLv3.

## Highlights so far

**Reading & navigation**
- **Custom render pipeline** — pages are laid out arithmetically and rendered
  off the UI thread into an LRU cache, so even a 4000-page document opens
  instantly and scrolls smoothly.
- **Tabbed workspace** — one tab per document, each remembering its place.
- **Home start screen** — recent files, drag-and-drop, one big Open.
- **Thumbnails & outline** — lazy off-thread thumbnails and an **editable**
  bookmark tree (add / rename / delete).
- **Find** — live match count with highlight and next/previous navigation.
- **Immersive reading mode** and **native light/dark theming** (feather-teal
  accent, symbolic icons).

**Pages** — rotate, delete, drag-reorder, **insert** pages from another PDF,
**extract** a page range, and **crop** margins — all lossless via QPDF.
Combine, split, compare, **granular optimize** (audit by category, image
downsample, font unembed), watermark, Bates numbering, RGB→CMYK.

**Annotations** — highlight, sticky note, freehand ink, **underline**,
**strike-through**, **rectangle**, **line**, **arrow**, and **text boxes**,
with a comment sidebar. Import/export form data as **XFDF**.

**Forms** — fill text, checkbox, and dropdown fields, and **author** new ones
(text, checkbox, dropdown, radio, push button) by drawing them on the page;
move or delete existing fields. **Prepare Form** auto-detects the blanks and
checkboxes on a flat PDF and turns them into real fields in one reviewed pass,
with date / number / currency input formatting.

**Editing** — edit the text you've added; add, edit, and remove **hyperlinks**;
an **Open in LibreOffice Draw** bridge for heavier layout work.

**Review tools** — **measure** distances, perimeters, and areas; **snapshot** a
page region to the clipboard or a PNG; a **stamp library** (Approved / Draft /
Confidential / For Review, with date and name).

**Signing** — sign with a **text or graphical (image) appearance**, verify
existing signatures (PAdES/NSS), attach an optional **RFC 3161 trusted
timestamp**, and add **long-term validation (LTV / PAdES-LTA)** — embed each
signature's certificate chain (plus OCSP/CRL when available) in a **/DSS**, and
optionally an embedded **archive timestamp** (**/DocTimeStamp**), so signatures
keep validating long after the certificate expires.

**More** — **scan straight to PDF** from any SANE device (with optional OCR in
the same pass); OCR (Tesseract) and image→PDF, with optional **deskew /
despeckle / binarize** pre-processing and **automatic language detection**;
office/image conversion (LibreOffice), **export back to Word/ODT/RTF/text** or **to
PNG/JPEG/TIFF images**, AES-256 encryption, redaction (manual or **pattern-based
Find & Redact**), **sanitize / remove hidden info**, watermarks,
**headers/footers & page numbers**, **visual or word-level compare**,
**PDF/A-1b tagging + veraPDF preflight**, and flattening. **Read aloud** speaks
the document sentence by sentence via speech-dispatcher — fully local, nothing
leaves your machine. A **batch / action wizard** composes any of these
operations into a pipeline and runs it over many files at once.

**Command line** — the same binary runs **headless** when given a sub-command,
for scripts, pipelines, and servers (no display needed):

```sh
feather-pdf merge out.pdf a.pdf b.pdf
feather-pdf extract in.pdf out.pdf --pages 1-3,5
feather-pdf encrypt in.pdf out.pdf --password secret --no-print
feather-pdf optimize in.pdf out.pdf --dpi 150
feather-pdf ocr scan.pdf searchable.pdf --lang eng
feather-pdf thumbnail in.pdf cover.png --size 256
feather-pdf batch in.pdf out.pdf --action my-action.json
feather-pdf watch --action my-action.json --in ~/Inbox --out ~/Done
feather-pdf --help          # merge · split · rotate · watermark · bates · sanitize · images · batch · watch · …
```

**Folder automation** — assemble a multi-step *action* in the Batch wizard,
**Save action…**, then run it over a whole folder. `feather-pdf watch` keeps
watching and processes each PDF as it arrives — point a systemd user service
(example shipped under `share/feather-pdf/systemd/`) at an inbox to automate it.

**Desktop integration** — installing registers Feather as a PDF handler
(*Open With…*) and as a **thumbnailer**, so GNOME Files / Dolphin show a
page-one preview for every PDF instead of a generic icon (it shells out to
`feather-pdf thumbnail`, rendering in-process with no external tools). It also
adds a **right-click "Feather PDF" menu** — *Compress*, *Remove Hidden Info*,
*Extract Text* — for GNOME Files (via a nautilus-python extension) and Dolphin
(via a KDE service menu); each runs headless and drops the result beside the
source with a desktop notification.

A **"Print to Feather PDF" virtual printer** is available too: run
`sudo feather-pdf-setup-printer` once, then *Print* from any app to capture the
output as a PDF in `~/Documents/Feather PDF/`, which opens in Feather PDF.
(`feather-pdf-setup-printer --uninstall` removes it.)

## Install

Prebuilt packages (x86_64) are on the
[**releases page**](https://github.com/s4rt4/featherpdf-linux/releases).

Fedora (and other RPM distros):

```sh
wget https://github.com/s4rt4/featherpdf-linux/releases/download/v0.0.1/feather-pdf-0.0.1-1.x86_64.rpm
sudo dnf install ./feather-pdf-0.0.1-1.x86_64.rpm
```

Debian / Ubuntu:

```sh
wget https://github.com/s4rt4/featherpdf-linux/releases/download/v0.0.1/feather-pdf_0.0.1_amd64.deb
sudo apt install ./feather-pdf_0.0.1_amd64.deb
```

Dependencies (Qt 6.8+, Poppler-Qt6, QPDF 12) are pulled in automatically by
`dnf`/`apt`. On other distributions, build from source below (packages are
generated with CPack: `cpack -G RPM` / `cpack -G DEB` from the build dir).

## Building

Feather targets **Fedora / GNOME (Wayland)** first. On Fedora 43:

```sh
sudo dnf install -y gcc-c++ cmake ninja-build \
    qt6-qtbase-devel qt6-qtpdf-devel poppler-qt6-devel qt6-qtsvg-devel \
    qt6-qttools-devel clang-tools-extra

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/src/feather-pdf path/to/document.pdf
```

Requires a C++20 compiler and Qt 6.5 or newer.

## Roadmap

| Milestone | Focus | Status |
|-----------|-------|--------|
| M0 | Foundation & viewer | ✅ Done |
| M1 | Page operations (lossless, via QPDF) | ✅ Done |
| M2 | Security & redaction | ✅ Done |
| M3 | Annotations (+ XFDF form data) | ✅ Done |
| M4 | Forms — fill **and** author | ✅ Done |
| M5 | OCR & image→PDF | ✅ Done |
| M6 | Signatures (PAdES) | ✅ Done |
| M7 | Conversion (LibreOffice, images) | ✅ Done |
| M8 | Editing — text you added ✅ · existing-text reflow & images *(the frontier)* | ◐ In progress |
| M9 | Polish, plugins, i18n, CLI | 🔜 Next |

## License

Feather PDF is licensed under the **GNU General Public License v3.0**.
See [LICENSE](LICENSE).
