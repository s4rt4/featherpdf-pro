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

#include <cstdint>

// Shrinks a PDF: audits where the bytes live, downsamples oversized images,
// optionally unembeds fonts, and recompresses streams - all in-process with
// QPDF + QImage (no Ghostscript, no AGPL dependencies).
class Optimizer {
public:
    // Bytes attributable to each category of stored object, plus the file total.
    // Categories are based on the raw (stored, still-compressed) stream sizes, so
    // they line up with what actually occupies space on disk.
    struct Audit {
        qint64 images = 0;         // image XObjects
        qint64 fonts = 0;          // embedded font programs (FontFile/2/3)
        qint64 content = 0;        // page content streams
        qint64 metadata = 0;       // XMP / metadata streams
        qint64 other = 0;          // everything else (overhead, fallback)
        qint64 total = 0;          // whole file on disk
    };

    struct Options {
        int targetDpi = 150;       // downsample images above this; <= 0 disables it
        bool dropFonts = false;    // strip embedded font programs (unembed)
        bool compress = true;      // object streams + recompress (the classic optimize)
    };

    // What actually changed, for the confirmation toast.
    struct Report {
        qint64 beforeBytes = 0;
        qint64 afterBytes = 0;
        int imagesDownsampled = 0;
        int imagesSkipped = 0;
        int fontsDropped = 0;
    };

    // Walk the document and report bytes by category. Returns false on read error.
    static bool audit(const QString& inputPath, Audit* out, QString* error);

    // Produce an optimized copy at outputPath (may equal inputPath to rewrite in
    // place). Returns false and fills error on failure.
    static bool optimize(const QString& inputPath, const QString& outputPath,
                         const Options& options, Report* report, QString* error);
};
