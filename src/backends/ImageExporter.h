// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <QString>
#include <QVector>

class QPdfDocument;

// Renders PDF pages to raster image files, in-process via QPdfDocument (the same
// PDFium renderer used on screen) - no external tools needed.
class ImageExporter {
public:
    enum class Format { Png, Jpeg, Tiff };

    // One page to render: which QPdfDocument page, the session rotation to apply
    // (0/90/180/270), and the 1-based number to use in the output file name.
    struct PageSpec {
        int sourcePage;
        int rotationDeg;
        int label;
    };

    // Render each spec at `dpi` to `outDir`/`baseName`-NN.<ext>, flattened onto
    // white. Returns the number of files written; sets *error and returns 0 on
    // failure.
    static int renderPages(QPdfDocument* doc, const QVector<PageSpec>& pages, const QString& outDir,
                           const QString& baseName, Format fmt, int dpi, QString* error);

    // Render page one of `inputPath` to a PNG at `outPath`, scaled so its longest
    // edge is `size` pixels (the freedesktop thumbnailer convention), flattened
    // onto white. Loads the document itself. Returns false and sets *error on
    // failure (missing/locked/empty PDF, unrenderable page, unwritable output).
    static bool renderThumbnail(const QString& inputPath, const QString& outPath, int size,
                                QString* error);

    // True if `pdfimages` (Poppler util) is installed for embedded-image extraction.
    static bool hasImageExtractor();

    // Extract the raster images embedded in `inputPath` into `outDir` as
    // `baseName`-NNN.<ext> via `pdfimages -all`, keeping each image's own format.
    // Returns the number of files written. Returns 0 with *error empty when the
    // PDF simply has no embedded images; sets *error and returns 0 on real failure.
    static int extractEmbedded(const QString& inputPath, const QString& outDir,
                               const QString& baseName, QString* error);
};
