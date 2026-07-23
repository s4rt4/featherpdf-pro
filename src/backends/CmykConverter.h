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

// Converts a PDF's colors from RGB (and gray) to process CMYK for prepress -
// the colorspace commercial printers expect. Orchestrates Ghostscript's
// pdfwrite device, which rewrites every colorspace, image, and shading to
// DeviceCMYK while leaving text, vectors, and structure intact.
class CmykConverter {
public:
    // Whether Ghostscript ('gs') is installed and usable.
    static bool isAvailable();

    // Rewrite inputPath into outputPath with all colors converted to DeviceCMYK.
    // Returns false and fills *error on failure.
    static bool toCmyk(const QString& inputPath, const QString& outputPath, QString* error);
};
