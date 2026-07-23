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
#include <QRectF>
#include <QString>

// M8 stage (a): edit the text *you added*. A text box added with the Text tool
// is a /FreeText annotation, which we own end to end — so we can re-open it,
// change its words, and regenerate its appearance. (Editing the document's
// pre-existing body text with reflow is the harder M8 stage (b), still R&D; the
// LibreOffice Draw bridge is the interim fallback for heavy edits.)
class TextEditor {
public:
    struct TextBox {
        int page = 0;   // 0-based page index
        QRectF rect;    // normalized [0,1] page rect (top-left origin)
        QString text;   // current contents
        int objId = 0;  // QPDF object id/gen — a stable handle within one load
        int objGen = 0;
    };

    // Every editable text box (/FreeText annotation) in `path`, in page order.
    static QList<TextBox> read(const QString& path);

    // Replace the contents of the box identified by `objId`/`objGen` with `text`
    // and regenerate its appearance. Temp file + atomic rename. Returns true on
    // success; on failure fills *error.
    static bool setText(const QString& inputPath, const QString& outputPath, int objId, int objGen,
                        const QString& text, QString* error);

    // Remove the box identified by `objId`/`objGen` from its page. Temp file +
    // atomic rename. Returns true on success; on failure fills *error.
    static bool remove(const QString& inputPath, const QString& outputPath, int objId, int objGen,
                       QString* error);
};
