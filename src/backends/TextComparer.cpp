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

#include "backends/TextComparer.h"

#include <QMultiMap>
#include <QRegularExpression>

#include <algorithm>
#include <memory>
#include <vector>

#include <poppler-qt6.h>

namespace {
QStringList tokenize(const QString& text) {
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    return text.split(ws, Qt::SkipEmptyParts);
}

// Word-level diff of `a` (old) vs `b` (new) via a longest-common-subsequence
// backtrack, appending to `removed`/`added`. Guarded against pathologically long
// pages by falling back to a multiset difference.
void diffWords(const QStringList& a, const QStringList& b, QStringList& removed, QStringList& added) {
    const int n = a.size(), m = b.size();
    if (n == 0 && m == 0)
        return;
    // LCS is O(n*m); cap the table so a huge page can't blow up memory/time.
    if (qint64(n) * m > 4'000'000) {
        QMultiMap<QString, int> bag;
        for (const QString& w : b)
            bag.insert(w, 0);
        for (const QString& w : a) {
            auto it = bag.find(w);
            if (it != bag.end())
                bag.erase(it); // matched, not removed
            else
                removed << w;
        }
        for (auto it = bag.constBegin(); it != bag.constEnd(); ++it)
            added << it.key();
        return;
    }

    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
    for (int i = 1; i <= n; ++i)
        for (int j = 1; j <= m; ++j)
            dp[i][j] = (a[i - 1] == b[j - 1]) ? dp[i - 1][j - 1] + 1
                                              : std::max(dp[i - 1][j], dp[i][j - 1]);

    QStringList rev_removed, rev_added;
    int i = n, j = m;
    while (i > 0 && j > 0) {
        if (a[i - 1] == b[j - 1]) {
            --i;
            --j;
        } else if (dp[i - 1][j] >= dp[i][j - 1]) {
            rev_removed << a[i - 1];
            --i;
        } else {
            rev_added << b[j - 1];
            --j;
        }
    }
    while (i > 0)
        rev_removed << a[--i];
    while (j > 0)
        rev_added << b[--j];

    std::reverse(rev_removed.begin(), rev_removed.end());
    std::reverse(rev_added.begin(), rev_added.end());
    removed += rev_removed;
    added += rev_added;
}

QStringList pageWords(Poppler::Document* doc, int page) {
    if (!doc || page >= doc->numPages())
        return {};
    std::unique_ptr<Poppler::Page> p = doc->page(page);
    return p ? tokenize(p->text(QRectF())) : QStringList{};
}
} // namespace

TextComparer::Result TextComparer::compare(const QString& oldPath, const QString& newPath,
                                           QString* error) {
    Result result;
    std::unique_ptr<Poppler::Document> oldDoc = Poppler::Document::load(oldPath);
    std::unique_ptr<Poppler::Document> newDoc = Poppler::Document::load(newPath);
    if (!oldDoc || oldDoc->isLocked() || !newDoc || newDoc->isLocked()) {
        if (error)
            *error = QStringLiteral("One of the PDFs couldn't be opened for comparison.");
        return result;
    }

    const int pages = std::max(oldDoc->numPages(), newDoc->numPages());
    result.pageCount = pages;
    for (int pg = 0; pg < pages; ++pg) {
        PageDiff diff;
        diff.page = pg;
        diffWords(pageWords(oldDoc.get(), pg), pageWords(newDoc.get(), pg), diff.removed,
                  diff.added);
        if (diff.changed()) {
            result.addedWords += diff.added.size();
            result.removedWords += diff.removed.size();
            result.pages.push_back(diff);
        }
    }
    return result;
}
