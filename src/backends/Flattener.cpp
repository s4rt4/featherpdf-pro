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

#include "backends/Flattener.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPageSize>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>

#include <exception>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>

bool Flattener::flattenLossless(const QString& inputPath, const QString& outputPath,
                                QString* error) {
    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());
        // Make sure form fields have appearance streams, then bake all annotation
        // (incl. widget) appearances into the page content and remove /Annots.
        QPDFAcroFormDocumentHelper(qpdf).generateAppearancesIfNeeded();
        QPDFPageDocumentHelper(qpdf).flattenAnnotations();
        QPDFWriter writer(qpdf, target.toLocal8Bit().constData());
        writer.setCompressStreams(true);
        writer.write();
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        if (inPlace)
            QFile::remove(target);
        return false;
    }

    if (inPlace) {
        QFile::remove(outputPath);
        if (!QFile::rename(target, outputPath)) {
            if (error)
                *error = QStringLiteral("The flattened file couldn't replace the original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}

bool Flattener::flattenRaster(const QString& inputPath, const QString& outputPath, int dpi,
                              QString* error) {
    QPdfDocument doc;
    if (doc.load(inputPath) != QPdfDocument::Error::None) {
        if (error)
            *error = QStringLiteral("The document couldn't be opened.");
        return false;
    }
    if (doc.pageCount() <= 0) {
        if (error)
            *error = QStringLiteral("The document has no pages.");
        return false;
    }

    const QString target = outputPath + QStringLiteral(".feather-tmp");
    {
        QPdfWriter writer(target);
        writer.setResolution(dpi);
        writer.setPageMargins(QMarginsF(0, 0, 0, 0));
        QPainter painter;
        bool started = false;

        QPdfDocumentRenderOptions opts;
        opts.setRenderFlags(QPdfDocumentRenderOptions::RenderFlag::Annotations);
        for (int i = 0; i < doc.pageCount(); ++i) {
            QSizeF pt = doc.pagePointSize(i);
            if (pt.width() <= 0 || pt.height() <= 0)
                continue;
            const QSize px(qRound(pt.width() * dpi / 72.0), qRound(pt.height() * dpi / 72.0));
            QImage flat(px, QImage::Format_RGB888);
            flat.fill(Qt::white);
            const QImage rendered = doc.render(i, px, opts);
            if (!rendered.isNull()) {
                QPainter pp(&flat);
                pp.drawImage(0, 0, rendered);
            }
            writer.setPageSize(QPageSize(pt, QPageSize::Point));
            if (!started) {
                if (!painter.begin(&writer)) {
                    if (error)
                        *error = QStringLiteral("Couldn't create the flattened PDF.");
                    return false;
                }
                started = true;
            } else {
                writer.newPage();
            }
            painter.drawImage(QRect(0, 0, writer.width(), writer.height()), flat);
        }
        painter.end();
        if (!started) {
            if (error)
                *error = QStringLiteral("No pages could be rendered.");
            QFile::remove(target);
            return false;
        }
    }

    QFile::remove(outputPath);
    if (!QFile::rename(target, outputPath)) {
        if (error)
            *error = QStringLiteral("The flattened file couldn't be saved.");
        QFile::remove(target);
        return false;
    }
    return true;
}
