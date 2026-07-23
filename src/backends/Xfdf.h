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

// Form-data interchange via XFDF (Adobe's XML form/annotation format). This
// covers the common case — exchanging AcroForm field values — reading and
// writing them through FormFiller (Poppler). Annotation interchange is a future
// extension.
class Xfdf {
public:
    // Write every AcroForm field value in `pdfPath` to `xfdfPath` as XFDF.
    // Returns true on success; on failure fills *error.
    static bool exportFields(const QString& pdfPath, const QString& xfdfPath, QString* error);

    // Apply the field values in `xfdfPath` to `pdfPath`, writing `outPath`.
    // Fields named in the XFDF but absent from the PDF are ignored. Returns true
    // on success; on failure fills *error.
    static bool importFields(const QString& pdfPath, const QString& xfdfPath,
                             const QString& outPath, QString* error);
};
