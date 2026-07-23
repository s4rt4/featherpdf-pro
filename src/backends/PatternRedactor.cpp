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

#include "backends/PatternRedactor.h"

#include <QRegularExpression>

#include <memory>
#include <vector>

#include <poppler-qt6.h>

namespace {
struct Word {
    int start; // offset of this word's text in the page string
    int len;
    QRectF box; // in points, top-left origin
};
}

QHash<int, QList<QRectF>> PatternRedactor::findMatches(const QString& inputPath,
                                                       const QRegularExpression& pattern,
                                                       int* total, QString* error) {
    QHash<int, QList<QRectF>> out;
    if (total)
        *total = 0;

    if (!pattern.isValid()) {
        if (error)
            *error = QStringLiteral("The search pattern is not valid.");
        return out;
    }

    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(inputPath);
    if (!doc || doc->isLocked()) {
        if (error)
            *error = QStringLiteral("The PDF couldn't be opened for searching.");
        return out;
    }

    const int pages = doc->numPages();
    for (int pg = 0; pg < pages; ++pg) {
        std::unique_ptr<Poppler::Page> page = doc->page(pg);
        if (!page)
            continue;
        const QSizeF size = page->pageSizeF();
        if (size.width() <= 0 || size.height() <= 0)
            continue;

        // Reconstruct the page text word-by-word, remembering where each word
        // lands in the string so a regex match maps back to bounding boxes.
        const std::vector<std::unique_ptr<Poppler::TextBox>> boxes = page->textList();
        QString text;
        QList<Word> words;
        words.reserve(int(boxes.size()));
        for (const auto& tb : boxes) {
            const QString w = tb->text();
            words.push_back({int(text.size()), int(w.size()), tb->boundingBox()});
            text += w;
            if (tb->hasSpaceAfter())
                text += QLatin1Char(' ');
        }
        if (text.isEmpty())
            continue;

        QList<QRectF> rects;
        auto matches = pattern.globalMatch(text);
        while (matches.hasNext()) {
            const QRegularExpressionMatch m = matches.next();
            const int ms = m.capturedStart();
            const int me = m.capturedEnd();
            if (me <= ms)
                continue;
            if (total)
                ++*total;
            // Every word that overlaps the match span gets redacted.
            for (const Word& word : words) {
                const int ws = word.start;
                const int we = word.start + word.len;
                if (ws < me && we > ms && word.box.width() > 0 && word.box.height() > 0)
                    rects.push_back(QRectF(word.box.x() / size.width(), word.box.y() / size.height(),
                                           word.box.width() / size.width(),
                                           word.box.height() / size.height()));
            }
        }
        if (!rects.isEmpty())
            out.insert(pg, rects);
    }

    return out;
}
