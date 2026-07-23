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

// Splits one PDF into several, losslessly (QPDF copies the page objects).
class Splitter {
public:
    enum class Mode {
        EachPage, // one file per page
        EveryN,   // groups of n consecutive pages
        Ranges,   // one file per range in a list like "1-3, 5, 8-10"
    };

    // Write the pieces into `outputDir` named "<baseName>-NN.pdf". `n` is used for
    // EveryN; `ranges` for Ranges. *filesWritten (optional) receives the count.
    static bool split(const QString& inputPath, const QString& outputDir, const QString& baseName,
                      Mode mode, int n, const QString& ranges, int* filesWritten, QString* error);
};
