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

// Compares two PDFs visually and writes a difference report PDF: each page is the
// newer document in grayscale with the pixels that changed from the older one
// painted red, so changes are obvious at a glance.
class Comparer {
public:
    // Compare `oldPath` (baseline) against `newPath` and write the diff to
    // `outputPath`. Blocking and slow - run it off the UI thread. `*changedPages`
    // (optional) receives how many pages differ. Returns true on success.
    static bool compare(const QString& oldPath, const QString& newPath, const QString& outputPath,
                        int* changedPages, QString* error);
};
