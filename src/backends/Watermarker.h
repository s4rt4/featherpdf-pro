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

#include <QColor>
#include <QString>

// Stamps text overlays onto pages with QPDF: a watermark (one centred, rotated
// label on every page) and Bates numbering (a sequential identifier in a corner
// of every page). Lossless - the original content is untouched, the stamp is an
// added overlay.
class Watermarker {
public:
    struct WatermarkOptions {
        QString text;
        QColor color = QColor(120, 120, 120);
        double opacity = 0.30;     // 0..1
        double fontSize = 64.0;    // points
        double rotationDeg = 45.0; // diagonal by default
    };

    // Add a centred, rotated watermark to every page.
    static bool addWatermark(const QString& inputPath, const QString& outputPath,
                             const WatermarkOptions& opts, QString* error);

    struct ImageWatermarkOptions {
        QString imagePath;
        double opacity = 0.30;     // 0..1
        double scale = 0.5;        // fraction of the page width
        double rotationDeg = 0.0;
    };

    // Add a centred image (e.g. a logo) to every page, honouring transparency.
    static bool addImageWatermark(const QString& inputPath, const QString& outputPath,
                                  const ImageWatermarkOptions& opts, QString* error);

    enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };

    struct BatesOptions {
        QString prefix;        // e.g. "ABC-"
        QString suffix;        // e.g. ""
        int start = 1;         // first number
        int digits = 6;        // zero-padded width
        QColor color = QColor(40, 40, 40);
        double fontSize = 10.0;
        Corner corner = Corner::BottomRight;
    };

    // Stamp a sequential Bates number on every page (start, start+1, …).
    static bool addBates(const QString& inputPath, const QString& outputPath,
                         const BatesOptions& opts, QString* error);

    struct HeaderFooterOptions {
        // Six independent slots. Each may contain tokens substituted per page:
        //   {n} page number · {p} total pages · {date} today · {file} file name
        QString headerLeft, headerCenter, headerRight;
        QString footerLeft, footerCenter, footerRight;
        QColor color = QColor(40, 40, 40);
        double fontSize = 10.0; // points
        double margin = 24.0;   // distance from the page edge, points
        int startNumber = 1;    // value of {n} on the first page
    };

    // Stamp header/footer text on every page, expanding the tokens above. The
    // file name shown by {file} comes from `inputPath`.
    static bool addHeaderFooter(const QString& inputPath, const QString& outputPath,
                                const HeaderFooterOptions& opts, QString* error);
};
