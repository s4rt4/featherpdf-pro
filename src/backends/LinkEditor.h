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

// Creates, edits, and removes hyperlink (/Link) annotations with QPDF.
class LinkEditor {
public:
    struct Link {
        int objId = 0;
        int objGen = 0;
        int page = 0;      // 0-based page index
        QRectF rect;       // normalized [0,1], top-left origin, page's natural orientation
        QString uri;       // URL for a URI link; empty otherwise
        bool isUri = false; // false = internal/GoTo link, shown read-only
    };

    // One change to apply to an existing link, identified by object id/generation.
    struct Edit {
        int objId = 0;
        int objGen = 0;
        bool remove = false; // delete the annotation (wins over newUri)
        QString newUri;      // replace the URI target
    };

    // Every /Link annotation in the document, in page order.
    static QList<Link> read(const QString& path);

    // Add a URL link covering `normRect` (displayed-page fractions) on 0-based
    // `page`. Produce-then-open style: writes `output`.
    static bool addUriLink(const QString& inputPath, const QString& outputPath, int page,
                           const QRectF& normRect, const QString& url, QString* error);

    // Apply a batch of deletions / URI changes in a single rewrite.
    static bool applyEdits(const QString& inputPath, const QString& outputPath,
                           const QList<Edit>& edits, QString* error);
};
