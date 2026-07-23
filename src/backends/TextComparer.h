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

#include <QList>
#include <QString>
#include <QStringList>

// Compares the text of two PDFs word by word (via Poppler), page against page,
// and reports the words added and removed on each page.
class TextComparer {
public:
    struct PageDiff {
        int page = 0;          // 0-based page index
        QStringList added;     // words present in the new document only
        QStringList removed;   // words present in the old document only
        bool changed() const { return !added.isEmpty() || !removed.isEmpty(); }
    };

    struct Result {
        QList<PageDiff> pages; // only pages that differ
        int pageCount = 0;     // max page count of the two documents
        int addedWords = 0;
        int removedWords = 0;
        bool changed() const { return !pages.isEmpty(); }
    };

    static Result compare(const QString& oldPath, const QString& newPath, QString* error);
};
