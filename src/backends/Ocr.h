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
#include <QStringList>

// The OCR backend (Tesseract). Adds an invisible, searchable/selectable text
// layer over a scanned (image-only) PDF without changing how it looks: each page
// is rendered to an image, Tesseract finds the words and their boxes, and the
// recognized text is written into the page in invisible render mode (Tr 3) at
// those positions via QPDF. Deliberately avoids Ghostscript/ocrmypdf (AGPL).
class QImage;

class Ocr {
public:
    // In-process clean-up applied to each rendered page (QImage only, no extra
    // system tools) before it reaches Tesseract, plus auto language detection.
    struct Options {
        bool deskew = true;       // straighten skewed scans
        bool despeckle = true;    // drop isolated specks
        bool binarize = true;     // grayscale + Otsu threshold to black/white
        bool autoLanguage = false; // detect the script and pick a language
    };

    static bool isAvailable();        // tesseract found on PATH
    static QStringList languages();   // installed Tesseract language codes (e.g. "eng")

    // Inspect `sample` with Tesseract's OSD (--psm 0), map the detected script to
    // an installed language and return it; returns `fallback` when detection is
    // unavailable or no installed language matches the script.
    static QString detectLanguage(const QImage& sample, const QString& fallback);

    // OCR `inputPath` and write `outputPath` with the text layer added, using
    // `language` (e.g. "eng" or "eng+ind") and the given preprocessing `options`.
    // Blocking and slow - run it off the UI thread. Returns true on success; on
    // failure fills *error.
    static bool addTextLayer(const QString& inputPath, const QString& outputPath,
                             const QString& language, const Options& options, QString* error);
};
