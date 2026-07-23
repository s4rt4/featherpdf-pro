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

#include <QImage>
#include <QRectF>
#include <QString>

class QPdfDocument;

// Renders a rectangular region of one PDF page to a QImage, in-process via
// QPdfDocument (the same PDFium renderer used on screen and by ImageExporter).
// Used by the Snapshot tool: drag a marquee, crop, copy to the clipboard or save.
class SnapshotExporter {
public:
    // Render `sourcePage` at `dpi` honouring `rotationDeg` (0/90/180/270), then
    // crop to `normRect` - fractions in [0,1] of the DISPLAYED (rotated) page -
    // and flatten onto white. Returns the cropped image; returns a null QImage and
    // sets *error on failure.
    static QImage renderRegion(QPdfDocument* doc, int sourcePage, int rotationDeg,
                               const QRectF& normRect, int dpi, QString* error = nullptr);
};
