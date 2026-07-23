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

// Removes hidden information from a PDF before sharing - metadata, attachments,
// and embedded scripts/actions - rewriting a clean copy with QPDF.
class Sanitizer {
public:
    struct Options {
        bool metadata = true;    // document Info dict + XMP metadata
        bool attachments = true; // embedded files + file-attachment annotations
        bool scripts = true;     // JavaScript names + document/page actions
    };

    // Counts of what was actually stripped, for the confirmation message.
    struct Report {
        int metadata = 0;
        int attachments = 0;
        int scripts = 0;
        int total() const { return metadata + attachments + scripts; }
    };

    static bool sanitize(const QString& inputPath, const QString& outputPath,
                         const Options& options, Report* report, QString* error);
};
