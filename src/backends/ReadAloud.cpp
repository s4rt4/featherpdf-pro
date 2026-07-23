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

#include "backends/ReadAloud.h"

#include <QRegularExpression>

#include <memory>

#include <poppler-qt6.h>

namespace {
// Collapse runs of whitespace (incl. the newlines Poppler inserts mid-paragraph)
// into single spaces so an utterance reads as flowing speech.
QString normaliseSpace(const QString& s) {
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    return s.simplified().replace(ws, QStringLiteral(" "));
}
} // namespace

QStringList ReadAloud::splitSentences(const QString& pageText) {
    QStringList out;
    if (pageText.trimmed().isEmpty())
        return out;

    // Break after sentence-ending punctuation (. ! ? … and their CJK forms)
    // followed by whitespace. The lookbehind keeps the punctuation on the
    // sentence it ends. Blank lines (paragraph breaks) are also split points.
    static const QRegularExpression boundary(
        QStringLiteral("(?<=[.!?…。！？])\\s+|\\n\\s*\\n"));

    for (const QString& piece : pageText.split(boundary, Qt::SkipEmptyParts)) {
        const QString chunk = normaliseSpace(piece);
        if (chunk.isEmpty())
            continue;
        // A "sentence" with no letters or digits (e.g. a row of dashes or page
        // furniture) isn't worth speaking.
        static const QRegularExpression hasWord(QStringLiteral("[\\p{L}\\p{N}]"));
        if (!chunk.contains(hasWord))
            continue;
        out << chunk;
    }
    return out;
}

QList<ReadAloud::Utterance> ReadAloud::utterances(const QString& path, QString* error) {
    QList<Utterance> out;

    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(path);
    if (!doc || doc->isLocked()) {
        if (error)
            *error = QObject::tr("Couldn't open the document to read it aloud.");
        return out;
    }

    const int pages = doc->numPages();
    for (int i = 0; i < pages; ++i) {
        std::unique_ptr<Poppler::Page> page = doc->page(i);
        if (!page)
            continue;
        for (const QString& sentence : splitSentences(page->text(QRectF())))
            out.append({sentence, i});
    }
    return out;
}
