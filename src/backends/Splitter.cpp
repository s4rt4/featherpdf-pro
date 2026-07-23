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

#include "backends/Splitter.h"

#include <QDir>
#include <QStringList>

#include <exception>
#include <vector>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>

namespace {
// Parse "1-3, 5, 8-10" into groups of 0-based page indices (one group per range).
std::vector<std::vector<int>> parseRanges(const QString& text, int pageCount) {
    std::vector<std::vector<int>> groups;
    for (QString part : text.split(',', Qt::SkipEmptyParts)) {
        part = part.trimmed();
        std::vector<int> g;
        if (part.contains('-')) {
            const QStringList e = part.split('-');
            if (e.size() != 2)
                continue;
            bool ok1 = false, ok2 = false;
            int a = e[0].trimmed().toInt(&ok1), b = e[1].trimmed().toInt(&ok2);
            if (!ok1 || !ok2)
                continue;
            if (a > b)
                std::swap(a, b);
            for (int p = a; p <= b; ++p)
                if (p >= 1 && p <= pageCount)
                    g.push_back(p - 1);
        } else {
            bool ok = false;
            const int p = part.toInt(&ok);
            if (ok && p >= 1 && p <= pageCount)
                g.push_back(p - 1);
        }
        if (!g.empty())
            groups.push_back(g);
    }
    return groups;
}
} // namespace

bool Splitter::split(const QString& inputPath, const QString& outputDir, const QString& baseName,
                     Mode mode, int n, const QString& ranges, int* filesWritten, QString* error) {
    try {
        QPDF source;
        source.processFile(inputPath.toLocal8Bit().constData());
        QPDFPageDocumentHelper srcDh(source);
        srcDh.pushInheritedAttributesToPage();
        std::vector<QPDFPageObjectHelper> pages = srcDh.getAllPages();
        const int total = static_cast<int>(pages.size());
        if (total == 0) {
            if (error)
                *error = QStringLiteral("The document has no pages.");
            return false;
        }

        // Build the list of page-index groups, one per output file.
        std::vector<std::vector<int>> groups;
        if (mode == Mode::Ranges) {
            groups = parseRanges(ranges, total);
        } else {
            const int step = mode == Mode::EveryN ? std::max(1, n) : 1;
            for (int start = 0; start < total; start += step) {
                std::vector<int> g;
                for (int i = start; i < std::min(total, start + step); ++i)
                    g.push_back(i);
                groups.push_back(g);
            }
        }
        if (groups.empty()) {
            if (error)
                *error = QStringLiteral("No pages were selected to split.");
            return false;
        }

        const QDir dir(outputDir);
        const int width = QString::number(groups.size()).size();
        int written = 0;
        for (size_t gi = 0; gi < groups.size(); ++gi) {
            QPDF out;
            out.emptyPDF();
            QPDFPageDocumentHelper outDh(out);
            for (int idx : groups[gi])
                outDh.addPage(pages[idx], /*first=*/false); // foreign copy from source

            const QString name =
                QStringLiteral("%1-%2.pdf")
                    .arg(baseName,
                         QStringLiteral("%1").arg(int(gi + 1), width, 10, QLatin1Char('0')));
            const QString outPath = dir.filePath(name);
            QPDFWriter writer(out, outPath.toLocal8Bit().constData());
            writer.write();
            ++written;
        }
        if (filesWritten)
            *filesWritten = written;
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }
    return true;
}
