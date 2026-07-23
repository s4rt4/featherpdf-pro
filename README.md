# Feather PDF Pro

**Light on the system, full-featured on PDF.**

Feather PDF Pro is an open-source, native PDF application for **Windows**,
aiming at Adobe Acrobat–class capability without a web view. It wears
Acrobat's *bones* — a tabbed workspace, a per-document command toolbar, a
contextual Tools pane — with a quiet, modern skin.

It shares its UI and PDF engine with [Feather PDF for
Linux](https://github.com/s4rt4/featherpdf-linux), but is a separate app: the
platform layer (speech, signing store, shell integration, packaging) is being
rebuilt on native Windows technology.

> Status: Windows port in progress (milestone **W0** — building & running on
> MSVC). Feature milestones M0–M7 from the Linux app are inherited; the
> Windows-specific work is tracked in the roadmap below.

## Philosophy

Orchestrate mature libraries behind one clean façade; don't reinvent a PDF
engine.

- **Render (pixels):** PDFium, via Qt's `QtPdf`
- **Semantic layer (text, search, forms, annotations):** Poppler-Qt6
- **Lossless structure (merge, split, rotate, encrypt, metadata):** QPDF
- OCR with Tesseract · conversion with LibreOffice · read-aloud with the
  Windows voices (SAPI) · signing with OpenSSL/NSS (moving to Windows CNG)

Feather deliberately avoids in-process AGPL libraries (MuPDF, Ghostscript) to
stay GPLv3.

## Highlights

**Reading & navigation** — custom off-thread render pipeline with an LRU cache
(4000-page documents open instantly), tabbed workspace, thumbnails, editable
outline, find with live match count, immersive reading mode, native light/dark
theming.

**Pages** — rotate, delete, drag-reorder, insert, extract, crop — all lossless
via QPDF. Combine, split, compare, granular optimize, watermark, Bates
numbering, RGB→CMYK.

**Annotations** — highlight, sticky note, ink, underline, strike-through,
shapes, arrows, text boxes, with a comment sidebar and XFDF import/export.

**Forms** — fill text/checkbox/dropdown fields, author new ones by drawing
them on the page, and **Prepare Form** auto-detection with input formatting.

**Editing** — edit the text you've added, hyperlink editing, and an *Open in
LibreOffice Draw* bridge for heavier layout work.

**Signing** — text or image signature appearances, verification, RFC 3161
trusted timestamps, and long-term validation (LTV / PAdES-LTA with /DSS and
document timestamps).

**More** — OCR (Tesseract) with pre-processing and language auto-detection,
image→PDF, office conversion via LibreOffice, export to Word/ODT/RTF/images,
AES-256 encryption, redaction (manual or pattern-based), sanitize, headers &
footers, visual or word-level compare, PDF/A-1b + veraPDF preflight,
flattening, **Read aloud** via the built-in Windows voices, and a batch /
action wizard that pipelines any of these over many files.

**Command line** — the same `feather-pdf.exe` runs headless when given a
sub-command, for scripts and automation:

```powershell
feather-pdf merge out.pdf a.pdf b.pdf
feather-pdf extract in.pdf out.pdf --pages 1-3,5
feather-pdf encrypt in.pdf out.pdf --password secret --no-print
feather-pdf ocr scan.pdf searchable.pdf --lang eng
feather-pdf watch --action my-action.json --in C:\Inbox --out C:\Done
feather-pdf --help
```

**Windows integration** — the installer registers Feather for *Open With…* on
PDFs and adds right-click actions (*Compress*, *Remove Hidden Info*) to
Explorer. Point Task Scheduler at `feather-pdf watch` to automate a folder.

## Building

Requirements: **Visual Studio 2022** (C++ workload), **CMake 3.21+**, **Qt
6.5+ MSVC kit** with the *Qt PDF* and *Qt Speech* modules, and
**vcpkg** at `C:\vcpkg`.

```powershell
# 1. C dependencies (one-time)
vcpkg install freetype libjpeg-turbo openjpeg libpng tiff lcms zlib qpdf openssl --triplet x64-windows

# 2. Poppler-Qt6 against your Qt (one-time; installs into deps/poppler)
.\scripts\build-poppler.ps1 -QtDir C:/Qt/6.8.3/msvc2022_64

# 3. Feather itself
cmake --preset windows-msvc
cmake --build --preset windows-msvc
.\build\src\RelWithDebInfo\feather-pdf.exe path\to\document.pdf
```

If your Qt lives elsewhere, adjust `CMAKE_PREFIX_PATH` in
`CMakePresets.json` (or a `CMakeUserPresets.json`).

Optional runtime tools, picked up automatically when installed: **Tesseract**
(OCR), **LibreOffice** (office conversion, found via the registry),
**Ghostscript** (RGB→CMYK), **veraPDF** (PDF/A preflight).

## Packaging

`cpack` from the build dir produces a portable ZIP (Qt DLLs bundled via
windeployqt). `packaging/windows/feather-pdf-pro.iss` builds the Inno Setup
installer with the file association and Explorer context-menu actions.

## Roadmap

| Milestone | Focus | Status |
|-----------|-------|--------|
| W0 | Build & run on Windows/MSVC (this port) | ◐ In progress |
| W1 | Windows-native signing (CNG / certificate store) | 🔜 |
| W2 | Scanning via WIA | 🔜 |
| W3 | Installer polish: bundled Tesseract, winget, Explorer thumbnails | 🔜 |
| M8 | Editing — existing-text reflow & images *(shared with Linux)* | ◐ |

## License

Feather PDF Pro is licensed under the **GNU General Public License v3.0**.
See [LICENSE](LICENSE).
