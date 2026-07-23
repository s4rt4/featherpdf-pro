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

// Moves a PDF toward PDF/A-1b (archival) and runs an optional external preflight.
//
// Tagging is best-effort and done in-process with QPDF: it embeds an sRGB ICC
// profile as a GTS_PDFA1 OutputIntent, writes an XMP packet carrying pdfaid:part
// and pdfaid:conformance (plus dc/xmp basics), sets /MarkInfo and a file /ID. It
// does NOT rewrite page content, so it cannot guarantee full conformance - the
// goal is to take a clean, simple PDF a long way toward PDF/A-1b.
//
// Preflight calls the external "verapdf" validator when it is installed and
// degrades gracefully (a clear "not installed" status) when it is not - the same
// optional-external-tool pattern ImageExporter uses for "pdfimages".
class PdfA {
public:
    // Convert toward PDF/A-1b, writing a tagged copy to outputPath. In-place
    // (input == output) is supported via a temp file. Returns false and sets
    // *error on failure.
    static bool convertToPdfA1b(const QString& inputPath, const QString& outputPath, QString* error);

    // True if the "verapdf" CLI is installed and on PATH.
    static bool hasValidator();

    // Result of a preflight run.
    struct PreflightReport {
        bool available = false;       // verapdf was found
        bool ran = false;            // the validator actually produced a verdict
        bool pass = false;           // the file is PDF/A-1b compliant
        QString profile;             // the flavour checked, e.g. "PDF/A-1B"
        QStringList violations;      // a few failed-rule lines (may be truncated)
        QString summary;             // human-readable one-liner
    };

    // Validate inputPath as PDF/A-1b via verapdf. When verapdf is absent the
    // report comes back with available=false and a clear summary (this is NOT an
    // error - the function still returns true). Returns false and sets *error
    // only when the validator was found but couldn't be run at all.
    static bool validate(const QString& inputPath, PreflightReport* report, QString* error);
};
