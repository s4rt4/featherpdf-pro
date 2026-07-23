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

#include "backends/Converter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <memory>

#include <poppler-qt6.h>

namespace {
const QStringList kImageExts{"png", "jpg", "jpeg", "bmp", "gif", "tif", "tiff", "webp"};

QString findSoffice() {
    QString s = QStandardPaths::findExecutable(QStringLiteral("soffice"));
    if (s.isEmpty())
        s = QStandardPaths::findExecutable(QStringLiteral("libreoffice"));
    return s;
}

// Run `soffice --headless --convert-to <target>` on `inputPath` inside a private
// temp profile, then copy the produced `<base>.<outExt>` file to `outputPath`.
// `outExt` is the extension LibreOffice writes (used to find its output file).
// `inFilter`, when non-empty, forces the import filter (e.g. "writer_pdf_import"
// so a PDF loads into Writer instead of Draw, which can't export text formats).
bool runSoffice(const QString& inputPath, const QString& outputPath, const QString& target,
                const QString& outExt, const QString& inFilter, QString* error) {
    const QString soffice = findSoffice();
    if (soffice.isEmpty()) {
        if (error)
            *error = QStringLiteral("LibreOffice isn't installed, so this can't be converted.");
        return false;
    }

    // Convert into an isolated temp dir with a private profile (so it works even
    // if a LibreOffice instance is already running).
    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        if (error)
            *error = QStringLiteral("Couldn't create a temporary working directory.");
        return false;
    }
    const QString outDir = tmp.path();
    const QString profile = QStringLiteral("file://") + outDir + QStringLiteral("/profile");

    QStringList args{QStringLiteral("--headless")};
    if (!inFilter.isEmpty())
        args << QStringLiteral("--infilter=") + inFilter;
    args << QStringLiteral("--convert-to") << target << QStringLiteral("--outdir") << outDir
         << QStringLiteral("-env:UserInstallation=") + profile << inputPath;

    QProcess proc;
    proc.start(soffice, args);
    if (!proc.waitForStarted(15000)) {
        if (error)
            *error = QStringLiteral("LibreOffice couldn't be started.");
        return false;
    }
    if (!proc.waitForFinished(120000) || proc.exitStatus() != QProcess::NormalExit ||
        proc.exitCode() != 0) {
        if (error)
            *error = QStringLiteral("The document couldn't be converted.");
        return false;
    }

    const QString produced =
        QDir(outDir).filePath(QFileInfo(inputPath).completeBaseName() + QStringLiteral(".") + outExt);
    if (!QFile::exists(produced)) {
        if (error)
            *error = QStringLiteral("LibreOffice produced no output.");
        return false;
    }
    QFile::remove(outputPath);
    if (!QFile::copy(produced, outputPath)) {
        if (error)
            *error = QStringLiteral("The converted file couldn't be saved.");
        return false;
    }
    return true;
}
}

bool Converter::isImage(const QString& path) {
    return kImageExts.contains(QFileInfo(path).suffix().toLower());
}

bool Converter::hasOfficeConverter() {
    return !QStandardPaths::findExecutable(QStringLiteral("soffice")).isEmpty() ||
           !QStandardPaths::findExecutable(QStringLiteral("libreoffice")).isEmpty();
}

bool Converter::imagesToPdf(const QStringList& images, const QString& outputPath, QString* error) {
    if (images.isEmpty()) {
        if (error)
            *error = QStringLiteral("No images to convert.");
        return false;
    }

    QPdfWriter writer(outputPath);
    writer.setResolution(150); // painter dots-per-inch
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter;
    bool started = false;

    for (const QString& path : images) {
        QImage img(path);
        if (img.isNull())
            continue;
        // Page sized to the image at the writer resolution.
        const QSizeF pagePt(img.width() / 150.0 * 72.0, img.height() / 150.0 * 72.0);
        writer.setPageSize(QPageSize(pagePt, QPageSize::Point));
        if (!started) {
            if (!painter.begin(&writer)) {
                if (error)
                    *error = QStringLiteral("Couldn't create the PDF.");
                return false;
            }
            started = true;
        } else {
            writer.newPage();
        }
        painter.drawImage(QRect(0, 0, writer.width(), writer.height()), img);
    }
    painter.end();

    if (!started) {
        if (error)
            *error = QStringLiteral("None of the images could be read.");
        QFile::remove(outputPath);
        return false;
    }
    return true;
}

bool Converter::officeToPdf(const QString& inputPath, const QString& outputPath, QString* error) {
    return runSoffice(inputPath, outputPath, QStringLiteral("pdf"), QStringLiteral("pdf"),
                      QString(), error);
}

bool Converter::pdfToOffice(const QString& inputPath, const QString& outputPath, QString* error) {
    const QString ext = QFileInfo(outputPath).suffix().toLower();

    // Plain text comes straight from Poppler (which we already link) - clean,
    // in-process, and far better than LibreOffice's text export for PDFs.
    if (ext == QLatin1String("txt"))
        return pdfToText(inputPath, outputPath, error);

    // A PDF loads into Draw by default, which can't write Writer formats, so force
    // the Writer PDF import filter and name the matching export filter explicitly.
    static const QHash<QString, QString> kExportFilter{
        {QStringLiteral("docx"), QStringLiteral("MS Word 2007 XML")},
        {QStringLiteral("odt"), QStringLiteral("writer8")},
        {QStringLiteral("rtf"), QStringLiteral("Rich Text Format")},
    };
    const auto it = kExportFilter.constFind(ext);
    if (it == kExportFilter.constEnd()) {
        if (error)
            *error = QStringLiteral("Unsupported export format.");
        return false;
    }
    const QString target = ext + QStringLiteral(":") + it.value();
    return runSoffice(inputPath, outputPath, target, ext, QStringLiteral("writer_pdf_import"),
                      error);
}

bool Converter::pdfToText(const QString& inputPath, const QString& outputPath, QString* error) {
    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(inputPath);
    if (!doc || doc->isLocked()) {
        if (error)
            *error = QStringLiteral("The PDF couldn't be opened for text extraction.");
        return false;
    }

    QString text;
    const int pages = doc->numPages();
    for (int i = 0; i < pages; ++i) {
        std::unique_ptr<Poppler::Page> page = doc->page(i);
        if (!page)
            continue;
        text += page->text(QRectF()); // whole-page text in reading order
        if (i + 1 < pages)
            text += QStringLiteral("\n\f"); // form feed between pages
    }

    QFile out(outputPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("The text file couldn't be written.");
        return false;
    }
    out.write(text.toUtf8());
    out.close();
    return true;
}
