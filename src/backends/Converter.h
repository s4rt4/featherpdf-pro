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

// The "Create PDF" backend. Turns images into a PDF natively (QPdfWriter) and
// office documents into a PDF via LibreOffice headless.
class Converter {
public:
    // True if `path`'s extension is an image we can place directly into a PDF.
    static bool isImage(const QString& path);
    // True if LibreOffice (soffice) is installed for document conversion.
    static bool hasOfficeConverter();

    // Place each image on its own page (sized to the image) and write `outputPath`.
    static bool imagesToPdf(const QStringList& images, const QString& outputPath, QString* error);

    // Convert one office document (docx/odt/xlsx/…) to `outputPath` via LibreOffice
    // headless. Blocking - run it off the UI thread. Returns true on success.
    static bool officeToPdf(const QString& inputPath, const QString& outputPath, QString* error);

    // Export a PDF to an editable document. The target is taken from `outputPath`'s
    // extension: .txt extracts text via Poppler; .docx/.odt/.rtf convert via
    // LibreOffice headless (Writer import). Blocking - run it off the UI thread.
    static bool pdfToOffice(const QString& inputPath, const QString& outputPath, QString* error);

private:
    // Extract a PDF's text to `outputPath` using Poppler (in-process).
    static bool pdfToText(const QString& inputPath, const QString& outputPath, QString* error);
};
