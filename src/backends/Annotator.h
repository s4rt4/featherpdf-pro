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

#include <QColor>
#include <QList>
#include <QPolygonF>
#include <QRectF>
#include <QString>

// The annotation-authoring backend (Poppler-Qt6). Adds real PDF annotations to
// the document and saves them so any viewer (including our QtPdf one, on reopen)
// shows them. Coordinates are normalized to the page (top-left origin, with any
// display rotation already undone by the caller).
class Annotator {
public:
    struct Highlight {
        int page = 0;    // original 0-based page index
        QRectF rect;     // normalized [0,1] page rect
        QColor color;    // the highlight colour
    };

    struct Note {
        int page = 0;       // original 0-based page index
        QPointF pos;        // normalized [0,1] top-left anchor of the note icon
        QString text;       // the note's contents
        QColor color;       // the note icon colour
    };

    struct Ink {
        int page = 0;             // original 0-based page index
        QList<QPolygonF> strokes; // each stroke = normalized [0,1] points
        QColor color;             // the pen colour
    };

    // A vector annotation: text-markup (underline / strike-through), a stroked
    // rectangle, a line/arrow given by two endpoints, or a free-text box.
    struct Shape {
        enum class Kind { Underline, StrikeOut, Rectangle, Line, Arrow, TextBox };
        int page = 0;  // original 0-based page index
        Kind kind = Kind::Rectangle;
        QRectF rect;   // normalized rect (top-left origin, unrotated) — box kinds
        QColor color;  // the stroke colour
        QPointF a, b;  // normalized endpoints (Line / Arrow)
        QString text;  // contents (TextBox)
    };

    // Add `highlights`, `notes`, `inks`, and `shapes` to `inputPath` and write the
    // result to `outputPath`. Temp file + atomic rename, so `outputPath` may equal
    // `inputPath`. Returns true on success; on failure fills *error.
    static bool saveAnnotations(const QString& inputPath, const QString& outputPath,
                                const QList<Highlight>& highlights, const QList<Note>& notes,
                                const QList<Ink>& inks, const QList<Shape>& shapes, QString* error);
};
