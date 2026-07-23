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

#include <QHash>
#include <QList>
#include <QRectF>
#include <QString>

class QRegularExpression;

// Finds text matching a pattern and reports where it sits, so the existing
// redaction pipeline can black it out. Uses Poppler for text + positions.
class PatternRedactor {
public:
    // For each 0-based page, the bounding rectangles of words whose text matches
    // `pattern`, normalized to 0..1 with a top-left origin in the page's natural
    // (baked-/Rotate) orientation - the same convention redaction marks use,
    // before any in-session rotation. *total (optional) gets the match count.
    static QHash<int, QList<QRectF>> findMatches(const QString& inputPath,
                                                 const QRegularExpression& pattern, int* total,
                                                 QString* error);
};
