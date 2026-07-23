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

#include "backends/Comparer.h"

#include <QFile>
#include <QImage>
#include <QPageSize>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>

namespace {
// Render a page onto white at a fixed pixel size (blank if the page is absent).
QImage renderPage(QPdfDocument& doc, int page, const QSize& px) {
    QImage flat(px, QImage::Format_RGB888);
    flat.fill(Qt::white);
    if (page >= 0 && page < doc.pageCount()) {
        const QImage r = doc.render(page, px);
        if (!r.isNull()) {
            QPainter p(&flat);
            p.drawImage(QRect(QPoint(0, 0), px), r);
        }
    }
    return flat;
}
} // namespace

bool Comparer::compare(const QString& oldPath, const QString& newPath, const QString& outputPath,
                       int* changedPages, QString* error) {
    QPdfDocument oldDoc, newDoc;
    if (oldDoc.load(oldPath) != QPdfDocument::Error::None ||
        newDoc.load(newPath) != QPdfDocument::Error::None) {
        if (error)
            *error = QStringLiteral("One of the documents couldn't be opened.");
        return false;
    }

    const int pages = std::max(oldDoc.pageCount(), newDoc.pageCount());
    if (pages <= 0) {
        if (error)
            *error = QStringLiteral("The documents have no pages.");
        return false;
    }

    const QString target = outputPath + QStringLiteral(".feather-tmp");
    constexpr double kDpi = 150.0;
    int changed = 0;

    {
        QPdfWriter writer(target);
        writer.setResolution(int(kDpi));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0));
        QPainter painter;
        bool started = false;

        for (int i = 0; i < pages; ++i) {
            // Size the page to the newer document (fall back to the old one).
            QSizeF pt = i < newDoc.pageCount() ? newDoc.pagePointSize(i)
                                               : oldDoc.pagePointSize(i);
            if (pt.width() <= 0 || pt.height() <= 0)
                continue;
            const QSize px(qRound(pt.width() * kDpi / 72.0), qRound(pt.height() * kDpi / 72.0));

            const QImage a = renderPage(oldDoc, i, px);
            const QImage b = renderPage(newDoc, i, px);

            // Grayscale of the new page, with changed pixels painted red.
            QImage diff(px, QImage::Format_RGB888);
            bool pageChanged = false;
            for (int y = 0; y < px.height(); ++y) {
                const uchar* ra = a.constScanLine(y);
                const uchar* rb = b.constScanLine(y);
                uchar* ro = diff.scanLine(y);
                for (int x = 0; x < px.width(); ++x) {
                    const int o = x * 3;
                    const int d = std::abs(ra[o] - rb[o]) + std::abs(ra[o + 1] - rb[o + 1]) +
                                  std::abs(ra[o + 2] - rb[o + 2]);
                    if (d > 60) {
                        ro[o] = 220;
                        ro[o + 1] = 30;
                        ro[o + 2] = 30; // red
                        pageChanged = true;
                    } else {
                        // Desaturated, lightened new-page pixel so red marks pop.
                        const int g = (rb[o] * 30 + rb[o + 1] * 59 + rb[o + 2] * 11) / 100;
                        const int light = 128 + g / 2;
                        ro[o] = ro[o + 1] = ro[o + 2] = uchar(light);
                    }
                }
            }
            if (pageChanged)
                ++changed;

            writer.setPageSize(QPageSize(pt, QPageSize::Point));
            if (!started) {
                if (!painter.begin(&writer)) {
                    if (error)
                        *error = QStringLiteral("Couldn't create the comparison PDF.");
                    return false;
                }
                started = true;
            } else {
                writer.newPage();
            }
            painter.drawImage(QRect(0, 0, writer.width(), writer.height()), diff);
        }
        painter.end();
        if (!started) {
            if (error)
                *error = QStringLiteral("Nothing could be compared.");
            QFile::remove(target);
            return false;
        }
    }

    QFile::remove(outputPath);
    if (!QFile::rename(target, outputPath)) {
        if (error)
            *error = QStringLiteral("The comparison PDF couldn't be saved.");
        QFile::remove(target);
        return false;
    }
    if (changedPages)
        *changedPages = changed;
    return true;
}
