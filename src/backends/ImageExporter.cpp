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

#include "backends/ImageExporter.h"

#include <QDir>
#include <QImage>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfDocumentRenderOptions>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <cmath>

namespace {
struct FormatInfo {
    const char* ext;
    const char* qtFormat;
};
FormatInfo formatInfo(ImageExporter::Format fmt) {
    switch (fmt) {
    case ImageExporter::Format::Jpeg: return {"jpg", "JPEG"};
    case ImageExporter::Format::Tiff: return {"tiff", "TIFF"};
    case ImageExporter::Format::Png: break;
    }
    return {"png", "PNG"};
}

QPdfDocumentRenderOptions::Rotation rotationFor(int deg) {
    switch (((deg % 360) + 360) % 360) {
    case 90: return QPdfDocumentRenderOptions::Rotation::Clockwise90;
    case 180: return QPdfDocumentRenderOptions::Rotation::Clockwise180;
    case 270: return QPdfDocumentRenderOptions::Rotation::Clockwise270;
    default: return QPdfDocumentRenderOptions::Rotation::None;
    }
}
}

int ImageExporter::renderPages(QPdfDocument* doc, const QVector<PageSpec>& pages,
                               const QString& outDir, const QString& baseName, Format fmt, int dpi,
                               QString* error) {
    if (!doc || doc->pageCount() <= 0) {
        if (error)
            *error = QStringLiteral("No document to export.");
        return 0;
    }
    if (pages.isEmpty()) {
        if (error)
            *error = QStringLiteral("No pages selected.");
        return 0;
    }
    QDir dir(outDir);
    if (!dir.exists()) {
        if (error)
            *error = QStringLiteral("The output folder doesn't exist.");
        return 0;
    }
    dpi = qBound(36, dpi, 1200);
    const FormatInfo info = formatInfo(fmt);

    // Zero-pad the index to the widest label (at least 2: "-01").
    int maxLabel = 1;
    for (const PageSpec& s : pages)
        maxLabel = qMax(maxLabel, s.label);
    const int width = qMax(2, int(std::floor(std::log10(maxLabel))) + 1);

    int written = 0;
    for (const PageSpec& s : pages) {
        if (s.sourcePage < 0 || s.sourcePage >= doc->pageCount())
            continue;
        QSizeF pt = doc->pagePointSize(s.sourcePage);
        const bool quarter = (((s.rotationDeg % 360) + 360) % 360) == 90 ||
                             (((s.rotationDeg % 360) + 360) % 360) == 270;
        if (quarter)
            pt.transpose();
        if (pt.width() <= 0 || pt.height() <= 0)
            continue;
        // points (1/72") -> device pixels at the requested DPI.
        const QSize px(qRound(pt.width() * dpi / 72.0), qRound(pt.height() * dpi / 72.0));

        QPdfDocumentRenderOptions opts;
        opts.setRenderFlags(QPdfDocumentRenderOptions::RenderFlag::Annotations);
        opts.setRotation(rotationFor(s.rotationDeg));
        const QImage rendered = doc->render(s.sourcePage, px, opts);
        if (rendered.isNull())
            continue;

        // Flatten onto white - the render is transparent, and JPEG has no alpha.
        QImage img(rendered.size(), QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter painter(&img);
        painter.drawImage(0, 0, rendered);
        painter.end();

        const QString name = QStringLiteral("%1-%2.%3")
                                 .arg(baseName)
                                 .arg(s.label, width, 10, QChar('0'))
                                 .arg(QLatin1String(info.ext));
        if (img.save(dir.filePath(name), info.qtFormat))
            ++written;
    }

    if (written == 0 && error)
        *error = QStringLiteral("None of the pages could be rendered.");
    return written;
}

bool ImageExporter::renderThumbnail(const QString& inputPath, const QString& outPath, int size,
                                    QString* error) {
    size = qBound(16, size, 2048);
    QPdfDocument doc;
    if (doc.load(inputPath) != QPdfDocument::Error::None || doc.pageCount() <= 0) {
        if (error)
            *error = QStringLiteral("Couldn't open '%1'.").arg(inputPath);
        return false;
    }
    const QSizeF pt = doc.pagePointSize(0);
    if (pt.width() <= 0 || pt.height() <= 0) {
        if (error)
            *error = QStringLiteral("The first page has no size.");
        return false;
    }
    // Scale so the longest edge lands exactly on `size` px (the thumbnailer
    // convention); the short edge keeps the page's aspect ratio.
    const double scale = size / qMax(pt.width(), pt.height());
    const QSize px(qMax(1, qRound(pt.width() * scale)), qMax(1, qRound(pt.height() * scale)));

    QPdfDocumentRenderOptions opts;
    opts.setRenderFlags(QPdfDocumentRenderOptions::RenderFlag::Annotations);
    const QImage rendered = doc.render(0, px, opts);
    if (rendered.isNull()) {
        if (error)
            *error = QStringLiteral("The first page couldn't be rendered.");
        return false;
    }
    // Flatten onto white - the render is transparent.
    QImage img(rendered.size(), QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter painter(&img);
    painter.drawImage(0, 0, rendered);
    painter.end();
    if (!img.save(outPath, "PNG")) {
        if (error)
            *error = QStringLiteral("Couldn't write '%1'.").arg(outPath);
        return false;
    }
    return true;
}

bool ImageExporter::hasImageExtractor() {
    return !QStandardPaths::findExecutable(QStringLiteral("pdfimages")).isEmpty();
}

int ImageExporter::extractEmbedded(const QString& inputPath, const QString& outDir,
                                   const QString& baseName, QString* error) {
    const QString tool = QStandardPaths::findExecutable(QStringLiteral("pdfimages"));
    if (tool.isEmpty()) {
        if (error)
            *error = QStringLiteral("Extracting embedded images needs pdfimages (Poppler).");
        return 0;
    }
    QDir od(outDir);
    if (!od.exists()) {
        if (error)
            *error = QStringLiteral("The output folder doesn't exist.");
        return 0;
    }

    // Extract into a private temp dir first, then move the results out, so the
    // count is exact and we never clobber unrelated files in the target folder.
    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        if (error)
            *error = QStringLiteral("Couldn't create a temporary working directory.");
        return 0;
    }

    QProcess proc;
    // -all keeps each image's native format (JPEG stays JPEG, others become PNG).
    proc.start(tool, {QStringLiteral("-all"), inputPath, QDir(tmp.path()).filePath(baseName)});
    if (!proc.waitForStarted(15000)) {
        if (error)
            *error = QStringLiteral("pdfimages couldn't be started.");
        return 0;
    }
    if (!proc.waitForFinished(120000) || proc.exitStatus() != QProcess::NormalExit ||
        proc.exitCode() != 0) {
        if (error)
            *error = QStringLiteral("The images couldn't be extracted.");
        return 0;
    }

    const QFileInfoList produced =
        QDir(tmp.path()).entryInfoList(QStringList{baseName + QStringLiteral("-*")}, QDir::Files,
                                       QDir::Name);
    int moved = 0;
    for (const QFileInfo& f : produced) {
        const QString dst = od.filePath(f.fileName());
        QFile::remove(dst);
        if (QFile::copy(f.absoluteFilePath(), dst))
            ++moved;
    }
    // moved == 0 with no error means the PDF simply had no embedded images.
    return moved;
}
