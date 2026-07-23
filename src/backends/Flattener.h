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

// Flattens a PDF - making annotations and form fields non-interactive - in two
// ways the caller can choose between.
class Flattener {
public:
    // Lossless: bake annotation and form-field appearances into the page content
    // and drop the interactive objects. Text stays selectable. (QPDF)
    static bool flattenLossless(const QString& inputPath, const QString& outputPath,
                                QString* error);

    // Raster: render every page to an image and rebuild the document from those
    // images. Guarantees a flat result but loses selectable text. (QtPdf + QPDF)
    static bool flattenRaster(const QString& inputPath, const QString& outputPath, int dpi,
                              QString* error);
};
