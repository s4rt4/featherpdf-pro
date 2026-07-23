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

#include "backends/FormDetector.h"

#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <memory>
#include <vector>

#include <poppler-form.h>
#include <poppler-qt6.h>

using Type = FormEditor::Type;
using Format = FormEditor::Format;

namespace {
struct Word {
    QString text;
    QRectF box; // points, top-left origin
    int start;  // offset in the line string
    int len;
};

// Tidy a label fragment into a field name: trim, drop a trailing colon/dash,
// keep the last few words, and remove '.' (which PDF uses for name hierarchy).
QString labelToName(QString s) {
    s = s.simplified();
    while (!s.isEmpty() &&
           (s.back() == QLatin1Char(':') || s.back() == QLatin1Char('-') ||
            s.back() == QLatin1Char('.') || s.back() == QLatin1Char(')') ||
            s.back() == QLatin1Char(',')))
        s.chop(1);
    s = s.trimmed();
    QStringList w = s.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (w.size() > 6)
        w = w.mid(w.size() - 6);
    s = w.join(QLatin1Char(' '));
    s.replace(QLatin1Char('.'), QLatin1Char(' '));
    return s.simplified();
}

// Guess an input format from a Text field's label.
Format guessFormat(const QString& label) {
    const QString l = label.toLower();
    const auto has = [&](std::initializer_list<const char*> keys) {
        for (const char* k : keys)
            if (l.contains(QLatin1String(k)))
                return true;
        return false;
    };
    if (has({"date", "dob", "birth", "expiry", "expiration", "issued"}))
        return Format::Date;
    if (l.contains(QLatin1Char('%')) || has({"percent", "rate"}))
        return Format::Percent;
    if (l.contains(QLatin1Char('$')) ||
        has({"amount", "total", "price", "cost", "fee", "salary", "payment", "balance", "wage",
             "income", "subtotal", "tax"}))
        return Format::Currency;
    if (has({"quantity", "qty", "age", "count"}))
        return Format::Number;
    return Format::None;
}

// Union of the word boxes overlapping the half-open span [s, e) of the line.
QRectF spanBox(const QList<Word>& words, int s, int e) {
    QRectF r;
    for (const Word& w : words) {
        const int ws = w.start, we = w.start + w.len;
        if (ws < e && we > s && w.box.isValid())
            r = r.isNull() ? w.box : r.united(w.box);
    }
    return r;
}
} // namespace

QList<FormEditor::NewField> FormDetector::detect(const QString& inputPath, QString* error) {
    QList<FormEditor::NewField> out;

    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(inputPath);
    if (!doc || doc->isLocked()) {
        if (error)
            *error = QStringLiteral("The PDF couldn't be opened for scanning.");
        return out;
    }

    // Patterns: a run of 3+ underscores (optionally split by single spaces) is a
    // fill-in line; a bracket/ballot marker is a checkbox.
    static const QRegularExpression kRule(QStringLiteral("_{3,}(?:[ ]?_{3,})*"));
    static const QRegularExpression kCheck(QStringLiteral("\\[\\s*\\]|\\(\\s*\\)|[\\x{2610}\\x{25A1}]"));

    QSet<QString> usedNames;
    const auto uniqueName = [&](QString base, const char* fallback, int n) {
        if (base.isEmpty())
            base = QStringLiteral("%1_%2").arg(QLatin1String(fallback)).arg(n);
        QString name = base;
        int i = 2;
        while (usedNames.contains(name))
            name = QStringLiteral("%1_%2").arg(base).arg(i++);
        usedNames.insert(name);
        return name;
    };

    const int pages = doc->numPages();
    for (int pg = 0; pg < pages; ++pg) {
        std::unique_ptr<Poppler::Page> page = doc->page(pg);
        if (!page)
            continue;
        const QSizeF size = page->pageSizeF();
        if (size.width() <= 0 || size.height() <= 0)
            continue;
        const double W = size.width(), H = size.height();

        // Rects of any existing form fields, so we don't double up on a page that
        // already has widgets.
        QList<QRectF> existing;
        for (const auto& ff : page->formFields())
            existing.append(ff->rect());
        const auto overlapsExisting = [&](const QRectF& norm) {
            for (const QRectF& e : existing)
                if (e.intersects(norm))
                    return true;
            return false;
        };

        // Collect words with their boxes.
        std::vector<Word> raw;
        for (const auto& tb : page->textList()) {
            const QString t = tb->text();
            if (t.isEmpty())
                continue;
            raw.push_back({t, tb->boundingBox(), 0, 0});
        }
        if (raw.empty())
            continue;

        // Group words into lines by vertical overlap, then sort each line by x.
        std::sort(raw.begin(), raw.end(),
                  [](const Word& a, const Word& b) { return a.box.top() < b.box.top(); });
        std::vector<std::vector<Word>> lines;
        for (const Word& w : raw) {
            bool placed = false;
            if (!lines.empty()) {
                std::vector<Word>& cur = lines.back();
                const double cy = cur.front().box.center().y();
                const double tol = std::max(cur.front().box.height(), w.box.height()) * 0.6;
                if (std::abs(w.box.center().y() - cy) <= tol) {
                    cur.push_back(w);
                    placed = true;
                }
            }
            if (!placed)
                lines.push_back({w});
        }

        int detIdx = 0;
        for (std::vector<Word>& lineWords : lines) {
            std::sort(lineWords.begin(), lineWords.end(),
                      [](const Word& a, const Word& b) { return a.box.left() < b.box.left(); });

            // Build the line string with char offsets back to each word, and the
            // line's text height (the underscores are thin; labels set the scale).
            QString line;
            QList<Word> words;
            double lineH = 0;
            for (Word w : lineWords) {
                w.start = line.size();
                w.len = w.text.size();
                line += w.text;
                line += QLatin1Char(' '); // one separator keeps spans simple
                lineH = std::max(lineH, w.box.height());
                words.append(w);
            }
            if (lineH <= 0)
                lineH = 12;

            // Underscore fill-in lines → Text fields. Track the previous match end
            // so each field's label is only the text since the last field.
            int lastEnd = 0;
            auto rules = kRule.globalMatch(line);
            while (rules.hasNext()) {
                const QRegularExpressionMatch m = rules.next();
                const QRectF ub = spanBox(words, m.capturedStart(), m.capturedEnd());
                const QString label = line.mid(lastEnd, m.capturedStart() - lastEnd);
                lastEnd = m.capturedEnd();
                if (!ub.isValid() || ub.width() <= 1)
                    continue;
                // A field box sitting on the underscore baseline, one line tall.
                const double bottom = std::min(H, ub.bottom() + lineH * 0.12);
                const double top = std::max(0.0, bottom - lineH * 1.25);
                const QRectF norm(ub.left() / W, top / H, ub.width() / W, (bottom - top) / H);
                if (norm.width() <= 0 || norm.height() <= 0 || overlapsExisting(norm))
                    continue;
                FormEditor::NewField f;
                f.type = Type::Text;
                f.page = pg;
                f.rect = norm;
                f.name = uniqueName(labelToName(label), "Text", ++detIdx);
                f.format = guessFormat(label);
                out.append(f);
            }

            // Checkbox markers → CheckBox fields; the label is the text after it.
            auto checks = kCheck.globalMatch(line);
            while (checks.hasNext()) {
                const QRegularExpressionMatch m = checks.next();
                const QRectF cb = spanBox(words, m.capturedStart(), m.capturedEnd());
                if (!cb.isValid())
                    continue;
                const double side = std::max(cb.height(), lineH * 0.85);
                const double left = cb.left();
                const double top = cb.center().y() - side / 2;
                const QRectF norm(left / W, std::max(0.0, top) / H, side / W, side / H);
                if (norm.width() <= 0 || norm.height() <= 0 || overlapsExisting(norm))
                    continue;
                const QString after = line.mid(m.capturedEnd());
                FormEditor::NewField f;
                f.type = Type::CheckBox;
                f.page = pg;
                f.rect = norm;
                f.name = uniqueName(labelToName(after), "Check", ++detIdx);
                out.append(f);
            }
        }
    }

    return out;
}
