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

#include "cli/Cli.h"

#include "backends/BatchRunner.h"
#include "backends/Converter.h"
#include "backends/ImageExporter.h"
#include "backends/LtvSigner.h"
#include "backends/Ocr.h"
#include "backends/Optimizer.h"
#include "backends/PdfEditor.h"
#include "backends/Sanitizer.h"
#include "backends/Splitter.h"
#include "backends/WatchFolder.h"
#include "backends/Watermarker.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QPdfDocument>
#include <QSet>
#include <QTextStream>
#include <QTimer>

#include <memory>

#include <poppler-qt6.h>

namespace {

QTextStream& out() {
    static QTextStream s(stdout);
    return s;
}
QTextStream& err() {
    static QTextStream s(stderr);
    return s;
}
int fail(const QString& message) {
    err() << "feather-pdf: " << message << Qt::endl;
    return 1;
}
int usage(const QString& message) {
    err() << "feather-pdf: " << message << Qt::endl;
    return 2;
}

// Page count via the same renderer the app uses. Returns -1 on failure.
int pageCount(const QString& path, QString* why) {
    QPdfDocument doc;
    const QPdfDocument::Error e = doc.load(path);
    if (e == QPdfDocument::Error::IncorrectPassword || e == QPdfDocument::Error::None) {
        if (e != QPdfDocument::Error::None) {
            if (why)
                *why = QStringLiteral("the document is password protected; run 'decrypt' first");
            return -1;
        }
        return doc.pageCount();
    }
    if (why)
        *why = QStringLiteral("couldn't open '%1'").arg(path);
    return -1;
}

// Parse "1-3,5,8-10" into 1-based page numbers, in the given order, validated
// against `total`. Returns false (and fills *why) on malformed input or a page
// outside [1, total].
bool parseRanges(const QString& spec, int total, QVector<int>* pages, QString* why) {
    pages->clear();
    const QStringList parts = spec.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        *why = QStringLiteral("no pages given");
        return false;
    }
    for (QString part : parts) {
        part = part.trimmed();
        bool ok = false;
        if (part.contains(QLatin1Char('-'))) {
            const QStringList ab = part.split(QLatin1Char('-'));
            if (ab.size() != 2) {
                *why = QStringLiteral("bad page range '%1'").arg(part);
                return false;
            }
            const int a = ab[0].trimmed().toInt(&ok);
            if (!ok) {
                *why = QStringLiteral("bad page number in '%1'").arg(part);
                return false;
            }
            bool ok2 = false;
            const int b = ab[1].trimmed().toInt(&ok2);
            if (!ok2) {
                *why = QStringLiteral("bad page number in '%1'").arg(part);
                return false;
            }
            const int lo = qMin(a, b);
            const int hi = qMax(a, b);
            if (lo < 1 || hi > total) {
                *why = QStringLiteral("page range '%1' is outside 1-%2").arg(part).arg(total);
                return false;
            }
            for (int p = lo; p <= hi; ++p)
                pages->append(p);
        } else {
            const int p = part.toInt(&ok);
            if (!ok) {
                *why = QStringLiteral("bad page number '%1'").arg(part);
                return false;
            }
            if (p < 1 || p > total) {
                *why = QStringLiteral("page %1 is outside 1-%2").arg(p).arg(total);
                return false;
            }
            pages->append(p);
        }
    }
    return true;
}

QString baseNameOf(const QString& path) {
    const QString b = QFileInfo(path).completeBaseName();
    return b.isEmpty() ? QStringLiteral("page") : b;
}

// Set up a per-command parser. `cmdArgs` is the argument list with the command
// name at index 0. Returns false if the user asked for help or mis-typed an
// option (in which case *handledExit holds the exit code to return).
bool parseCommand(QCommandLineParser& p, const QStringList& cmdArgs, int* handledExit) {
    p.addHelpOption();
    if (!p.parse(cmdArgs)) {
        *handledExit = usage(p.errorText());
        return false;
    }
    if (p.isSet(QStringLiteral("help"))) {
        out() << p.helpText();
        *handledExit = 0;
        return false;
    }
    return true;
}

// ── Commands ────────────────────────────────────────────────────────────────

int cmdMerge(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Merge several PDFs into one."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PDF."));
    p.addPositionalArgument(QStringLiteral("inputs"), QStringLiteral("Source PDFs, in order."),
                            QStringLiteral("IN..."));
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() < 3)
        return usage(QStringLiteral("merge needs an output and at least two inputs"));
    const QString output = pos.first();
    const QStringList inputs = pos.mid(1);
    QString why;
    if (!PdfEditor::combine(inputs, output, &why))
        return fail(why);
    out() << "Merged " << inputs.size() << " files into " << output << Qt::endl;
    return 0;
}

int cmdSplit(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Split a PDF into several files."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Source PDF."));
    p.addPositionalArgument(QStringLiteral("outdir"), QStringLiteral("Output folder."));
    p.addOption({QStringLiteral("mode"),
                 QStringLiteral("each (one file per page), every, or ranges. Default: each."),
                 QStringLiteral("mode"), QStringLiteral("each")});
    p.addOption({QStringLiteral("n"), QStringLiteral("Pages per file for --mode every."),
                 QStringLiteral("n"), QStringLiteral("1")});
    p.addOption({QStringLiteral("ranges"), QStringLiteral("Range list for --mode ranges, e.g. 1-3,5."),
                 QStringLiteral("ranges")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("split needs an input PDF and an output folder"));
    const QString modeStr = p.value(QStringLiteral("mode"));
    Splitter::Mode mode;
    if (modeStr == QLatin1String("each"))
        mode = Splitter::Mode::EachPage;
    else if (modeStr == QLatin1String("every"))
        mode = Splitter::Mode::EveryN;
    else if (modeStr == QLatin1String("ranges"))
        mode = Splitter::Mode::Ranges;
    else
        return usage(QStringLiteral("--mode must be each, every, or ranges"));
    int written = 0;
    QString why;
    if (!Splitter::split(pos[0], pos[1], baseNameOf(pos[0]), mode,
                         p.value(QStringLiteral("n")).toInt(), p.value(QStringLiteral("ranges")),
                         &written, &why))
        return fail(why);
    out() << "Wrote " << written << " files to " << pos[1] << Qt::endl;
    return 0;
}

int cmdExtract(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Extract a page range into a new PDF."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Source PDF."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PDF."));
    p.addOption({QStringLiteral("pages"), QStringLiteral("Pages to keep, e.g. 1-3,5,8-10."),
                 QStringLiteral("pages")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("extract needs an input and an output PDF"));
    if (!p.isSet(QStringLiteral("pages")))
        return usage(QStringLiteral("extract needs --pages"));
    QString why;
    const int total = pageCount(pos[0], &why);
    if (total < 0)
        return fail(why);
    QVector<int> pages;
    if (!parseRanges(p.value(QStringLiteral("pages")), total, &pages, &why))
        return usage(why);
    QVector<int> order, rotations;
    for (int oneBased : pages) {
        order.append(oneBased - 1);
        rotations.append(0);
    }
    if (!PdfEditor::saveArrangement(pos[0], pos[1], order, rotations, &why))
        return fail(why);
    out() << "Extracted " << order.size() << " pages into " << pos[1] << Qt::endl;
    return 0;
}

int cmdRotate(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Rotate pages by a multiple of 90 degrees."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Source PDF."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PDF."));
    p.addOption({QStringLiteral("angle"), QStringLiteral("90, 180, or 270 (clockwise)."),
                 QStringLiteral("angle")});
    p.addOption({QStringLiteral("pages"),
                 QStringLiteral("Pages to rotate, e.g. 1-3,5. Default: all pages."),
                 QStringLiteral("pages")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("rotate needs an input and an output PDF"));
    const int angle = p.value(QStringLiteral("angle")).toInt();
    if (angle != 90 && angle != 180 && angle != 270)
        return usage(QStringLiteral("--angle must be 90, 180, or 270"));
    QString why;
    const int total = pageCount(pos[0], &why);
    if (total < 0)
        return fail(why);
    QSet<int> rotate; // 1-based pages to turn
    if (p.isSet(QStringLiteral("pages"))) {
        QVector<int> pages;
        if (!parseRanges(p.value(QStringLiteral("pages")), total, &pages, &why))
            return usage(why);
        for (int oneBased : pages)
            rotate.insert(oneBased);
    } else {
        for (int i = 1; i <= total; ++i)
            rotate.insert(i);
    }
    QVector<int> order, rotations;
    for (int i = 0; i < total; ++i) {
        order.append(i);
        rotations.append(rotate.contains(i + 1) ? angle : 0);
    }
    if (!PdfEditor::saveArrangement(pos[0], pos[1], order, rotations, &why))
        return fail(why);
    out() << "Rotated " << rotate.size() << " pages into " << pos[1] << Qt::endl;
    return 0;
}

int cmdEncrypt(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Encrypt a PDF with AES-256 and set permissions."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Source PDF."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PDF."));
    p.addOption({QStringLiteral("password"),
                 QStringLiteral("Open password. May be empty if you only restrict permissions."),
                 QStringLiteral("password"), QString()});
    p.addOption({QStringLiteral("no-print"), QStringLiteral("Disallow printing.")});
    p.addOption({QStringLiteral("no-copy"), QStringLiteral("Disallow copying text/graphics.")});
    p.addOption({QStringLiteral("no-edit"), QStringLiteral("Disallow editing/annotating.")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("encrypt needs an input and an output PDF"));
    PdfEditor::Permissions perms;
    perms.allowPrinting = !p.isSet(QStringLiteral("no-print"));
    perms.allowCopying = !p.isSet(QStringLiteral("no-copy"));
    perms.allowEditing = !p.isSet(QStringLiteral("no-edit"));
    const QString pw = p.value(QStringLiteral("password"));
    if (pw.isEmpty() && perms.allowPrinting && perms.allowCopying && perms.allowEditing)
        return usage(QStringLiteral("set --password or restrict at least one permission"));
    QString why;
    if (!PdfEditor::protect(pos[0], pos[1], pw, perms, &why))
        return fail(why);
    out() << "Wrote encrypted " << pos[1] << Qt::endl;
    return 0;
}

int cmdDecrypt(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Remove a PDF's password, writing an open copy."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Encrypted PDF."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PDF."));
    p.addOption({QStringLiteral("password"), QStringLiteral("The document's password."),
                 QStringLiteral("password")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("decrypt needs an input and an output PDF"));
    QString why;
    if (!PdfEditor::removeProtection(pos[0], pos[1], p.value(QStringLiteral("password")), &why))
        return fail(why);
    out() << "Wrote unprotected " << pos[1] << Qt::endl;
    return 0;
}

int cmdOptimize(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Shrink a PDF (recompress, downsample, unembed)."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Source PDF."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PDF."));
    p.addOption({QStringLiteral("dpi"),
                 QStringLiteral("Downsample images above this DPI. 0 disables. Default: 150."),
                 QStringLiteral("dpi"), QStringLiteral("150")});
    p.addOption({QStringLiteral("unembed-fonts"), QStringLiteral("Strip embedded font programs.")});
    p.addOption({QStringLiteral("no-compress"),
                 QStringLiteral("Skip object-stream packing and stream recompression.")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("optimize needs an input and an output PDF"));
    Optimizer::Options opts;
    opts.targetDpi = p.value(QStringLiteral("dpi")).toInt();
    opts.dropFonts = p.isSet(QStringLiteral("unembed-fonts"));
    opts.compress = !p.isSet(QStringLiteral("no-compress"));
    Optimizer::Report report;
    QString why;
    if (!Optimizer::optimize(pos[0], pos[1], opts, &report, &why))
        return fail(why);
    const double pct = report.beforeBytes > 0
        ? 100.0 * (report.beforeBytes - report.afterBytes) / report.beforeBytes
        : 0.0;
    out() << "Optimized " << pos[1] << ": " << report.beforeBytes << " -> " << report.afterBytes
          << " bytes (" << QString::number(pct, 'f', 1) << "% smaller)" << Qt::endl;
    return 0;
}

int cmdWatermark(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Stamp a diagonal text watermark on every page."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Source PDF."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PDF."));
    p.addOption({QStringLiteral("text"), QStringLiteral("Watermark text."), QStringLiteral("text")});
    p.addOption({QStringLiteral("opacity"), QStringLiteral("0..1. Default: 0.3."),
                 QStringLiteral("opacity"), QStringLiteral("0.3")});
    p.addOption({QStringLiteral("size"), QStringLiteral("Font size in points. Default: 64."),
                 QStringLiteral("size"), QStringLiteral("64")});
    p.addOption({QStringLiteral("angle"), QStringLiteral("Rotation in degrees. Default: 45."),
                 QStringLiteral("angle"), QStringLiteral("45")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("watermark needs an input and an output PDF"));
    if (!p.isSet(QStringLiteral("text")))
        return usage(QStringLiteral("watermark needs --text"));
    Watermarker::WatermarkOptions opts;
    opts.text = p.value(QStringLiteral("text"));
    opts.opacity = p.value(QStringLiteral("opacity")).toDouble();
    opts.fontSize = p.value(QStringLiteral("size")).toDouble();
    opts.rotationDeg = p.value(QStringLiteral("angle")).toDouble();
    QString why;
    if (!Watermarker::addWatermark(pos[0], pos[1], opts, &why))
        return fail(why);
    out() << "Watermarked " << pos[1] << Qt::endl;
    return 0;
}

int cmdBates(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Stamp sequential Bates numbers on every page."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Source PDF."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PDF."));
    p.addOption({QStringLiteral("prefix"), QStringLiteral("Text before the number."),
                 QStringLiteral("prefix"), QString()});
    p.addOption({QStringLiteral("suffix"), QStringLiteral("Text after the number."),
                 QStringLiteral("suffix"), QString()});
    p.addOption({QStringLiteral("start"), QStringLiteral("First number. Default: 1."),
                 QStringLiteral("start"), QStringLiteral("1")});
    p.addOption({QStringLiteral("digits"), QStringLiteral("Zero-padded width. Default: 6."),
                 QStringLiteral("digits"), QStringLiteral("6")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("bates needs an input and an output PDF"));
    Watermarker::BatesOptions opts;
    opts.prefix = p.value(QStringLiteral("prefix"));
    opts.suffix = p.value(QStringLiteral("suffix"));
    opts.start = p.value(QStringLiteral("start")).toInt();
    opts.digits = p.value(QStringLiteral("digits")).toInt();
    QString why;
    if (!Watermarker::addBates(pos[0], pos[1], opts, &why))
        return fail(why);
    out() << "Numbered " << pos[1] << Qt::endl;
    return 0;
}

int cmdSanitize(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(
        QStringLiteral("Strip hidden data: metadata, embedded files, and scripts."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Source PDF."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PDF."));
    p.addOption({QStringLiteral("keep-metadata"), QStringLiteral("Keep document metadata.")});
    p.addOption({QStringLiteral("keep-attachments"), QStringLiteral("Keep embedded files.")});
    p.addOption({QStringLiteral("keep-scripts"), QStringLiteral("Keep JavaScript and actions.")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("sanitize needs an input and an output PDF"));
    Sanitizer::Options opts;
    opts.metadata = !p.isSet(QStringLiteral("keep-metadata"));
    opts.attachments = !p.isSet(QStringLiteral("keep-attachments"));
    opts.scripts = !p.isSet(QStringLiteral("keep-scripts"));
    Sanitizer::Report report;
    QString why;
    if (!Sanitizer::sanitize(pos[0], pos[1], opts, &report, &why))
        return fail(why);
    out() << "Cleaned " << pos[1] << ": removed " << report.total() << " items (metadata "
          << report.metadata << ", attachments " << report.attachments << ", scripts "
          << report.scripts << ")" << Qt::endl;
    return 0;
}

int cmdLtv(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral(
        "Add long-term validation to a signed PDF: a Document Security Store holding "
        "each signature's certificate chain, plus OCSP/CRL responses when the "
        "certificates advertise them and the network is reachable. The existing "
        "signatures are left intact."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Signed source PDF."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PDF."));
    p.addOption({QStringLiteral("timestamp"),
                 QStringLiteral("Also embed a trusted archive timestamp (PAdES-LTA).")});
    p.addOption({QStringLiteral("tsa"),
                 QStringLiteral("Timestamp authority URL. Default: https://freetsa.org/tsr."),
                 QStringLiteral("url"), QStringLiteral("https://freetsa.org/tsr")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("ltv needs a signed input and an output PDF"));
    LtvSigner::Result res;
    QString why;
    if (!LtvSigner::addValidationInfo(pos[0], pos[1], &why, &res))
        return fail(why);
    out() << "Added long-term validation to " << pos[1] << ": " << res.certs
          << " certificate(s), " << res.ocsps << " OCSP, " << res.crls << " CRL across "
          << res.signatures << " signature(s)" << Qt::endl;

    if (p.isSet(QStringLiteral("timestamp"))) {
        const QString tmp = pos[1] + QStringLiteral(".ts.tmp");
        if (!LtvSigner::addDocumentTimestamp(pos[1], tmp, p.value(QStringLiteral("tsa")), &why)) {
            QFile::remove(tmp);
            return fail(why);
        }
        QFile::remove(pos[1]);
        if (!QFile::rename(tmp, pos[1])) {
            QFile::remove(tmp);
            return fail(QStringLiteral("Couldn't finalize the timestamped file"));
        }
        out() << "Embedded an archive timestamp from " << p.value(QStringLiteral("tsa"))
              << Qt::endl;
    }
    return 0;
}

int cmdOcr(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Add a searchable text layer to a scanned PDF."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Source PDF."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PDF."));
    p.addOption({QStringLiteral("lang"),
                 QStringLiteral("Tesseract language(s), e.g. eng or eng+ind. Default: eng."),
                 QStringLiteral("lang"), QStringLiteral("eng")});
    p.addOption({QStringLiteral("auto-language"), QStringLiteral("Detect the language automatically.")});
    p.addOption({QStringLiteral("no-deskew"), QStringLiteral("Don't straighten skewed scans.")});
    p.addOption({QStringLiteral("no-despeckle"), QStringLiteral("Don't remove specks.")});
    p.addOption({QStringLiteral("no-binarize"), QStringLiteral("Don't threshold to black and white.")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("ocr needs an input and an output PDF"));
    if (!Ocr::isAvailable())
        return fail(QStringLiteral("Tesseract isn't installed; install it to use OCR"));
    Ocr::Options opts;
    opts.deskew = !p.isSet(QStringLiteral("no-deskew"));
    opts.despeckle = !p.isSet(QStringLiteral("no-despeckle"));
    opts.binarize = !p.isSet(QStringLiteral("no-binarize"));
    opts.autoLanguage = p.isSet(QStringLiteral("auto-language"));
    QString why;
    if (!Ocr::addTextLayer(pos[0], pos[1], p.value(QStringLiteral("lang")), opts, &why))
        return fail(why);
    out() << "Recognized text into " << pos[1] << Qt::endl;
    return 0;
}

int cmdImages(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Render pages to PNG/JPEG/TIFF image files."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Source PDF."));
    p.addPositionalArgument(QStringLiteral("outdir"), QStringLiteral("Output folder."));
    p.addOption({QStringLiteral("dpi"), QStringLiteral("Resolution. Default: 150."),
                 QStringLiteral("dpi"), QStringLiteral("150")});
    p.addOption({QStringLiteral("format"), QStringLiteral("png, jpeg, or tiff. Default: png."),
                 QStringLiteral("format"), QStringLiteral("png")});
    p.addOption({QStringLiteral("pages"),
                 QStringLiteral("Pages to render, e.g. 1-3,5. Default: all pages."),
                 QStringLiteral("pages")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("images needs an input PDF and an output folder"));
    const QString fmtStr = p.value(QStringLiteral("format")).toLower();
    ImageExporter::Format fmt;
    if (fmtStr == QLatin1String("png"))
        fmt = ImageExporter::Format::Png;
    else if (fmtStr == QLatin1String("jpeg") || fmtStr == QLatin1String("jpg"))
        fmt = ImageExporter::Format::Jpeg;
    else if (fmtStr == QLatin1String("tiff") || fmtStr == QLatin1String("tif"))
        fmt = ImageExporter::Format::Tiff;
    else
        return usage(QStringLiteral("--format must be png, jpeg, or tiff"));

    auto doc = std::make_unique<QPdfDocument>();
    if (doc->load(pos[0]) != QPdfDocument::Error::None)
        return fail(QStringLiteral("couldn't open '%1'").arg(pos[0]));
    const int total = doc->pageCount();
    QVector<int> wanted; // 0-based
    if (p.isSet(QStringLiteral("pages"))) {
        QVector<int> oneBased;
        QString why;
        if (!parseRanges(p.value(QStringLiteral("pages")), total, &oneBased, &why))
            return usage(why);
        for (int n : oneBased)
            wanted.append(n - 1);
    } else {
        for (int i = 0; i < total; ++i)
            wanted.append(i);
    }
    QVector<ImageExporter::PageSpec> specs;
    int label = 1;
    for (int src : wanted)
        specs.append({src, 0, label++});
    QString why;
    const int written =
        ImageExporter::renderPages(doc.get(), specs, pos[1], baseNameOf(pos[0]), fmt,
                                   p.value(QStringLiteral("dpi")).toInt(), &why);
    if (written == 0)
        return fail(why.isEmpty() ? QStringLiteral("no pages were rendered") : why);
    out() << "Wrote " << written << " images to " << pos[1] << Qt::endl;
    return 0;
}

int cmdThumbnail(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(
        QStringLiteral("Render page one to a PNG thumbnail (for file managers)."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Source PDF."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PNG."));
    p.addOption({QStringLiteral("size"), QStringLiteral("Longest edge in pixels. Default: 256."),
                 QStringLiteral("size"), QStringLiteral("256")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("thumbnail needs an input PDF and an output PNG"));
    bool ok = false;
    const int size = p.value(QStringLiteral("size")).toInt(&ok);
    if (!ok || size <= 0)
        return usage(QStringLiteral("--size must be a positive number of pixels"));
    QString why;
    if (!ImageExporter::renderThumbnail(pos[0], pos[1], size, &why))
        return fail(why);
    out() << "Wrote " << pos[1] << Qt::endl;
    return 0;
}

int cmdToText(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Extract the text of a PDF to a .txt file."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Source PDF."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination .txt."));
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("to-text needs an input PDF and an output file"));
    // pdfToOffice routes a .txt target to in-process Poppler extraction (no
    // LibreOffice needed); nudge the user if they gave a different extension.
    if (!pos[1].endsWith(QLatin1String(".txt"), Qt::CaseInsensitive))
        return usage(QStringLiteral("to-text writes plain text; give an output ending in .txt"));
    QString why;
    if (!Converter::pdfToOffice(pos[0], pos[1], &why))
        return fail(why);
    out() << "Wrote " << pos[1] << Qt::endl;
    return 0;
}

int cmdToOffice(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(
        QStringLiteral("Convert a PDF to an editable document (.docx/.odt/.rtf) via LibreOffice."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Source PDF."));
    p.addPositionalArgument(QStringLiteral("output"),
                            QStringLiteral("Destination .docx/.odt/.rtf."));
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("to-office needs an input PDF and an output file"));
    if (!Converter::hasOfficeConverter())
        return fail(QStringLiteral("LibreOffice isn't installed; install it for this conversion"));
    QString why;
    if (!Converter::pdfToOffice(pos[0], pos[1], &why))
        return fail(why);
    out() << "Wrote " << pos[1] << " (layout is approximate)" << Qt::endl;
    return 0;
}

int cmdFromImages(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Build a PDF from image files, one page each."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PDF."));
    p.addPositionalArgument(QStringLiteral("images"), QStringLiteral("Image files, in order."),
                            QStringLiteral("IMG..."));
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() < 2)
        return usage(QStringLiteral("from-images needs an output PDF and at least one image"));
    QString why;
    if (!Converter::imagesToPdf(pos.mid(1), pos.first(), &why))
        return fail(why);
    out() << "Wrote " << pos.first() << " from " << (pos.size() - 1) << " images" << Qt::endl;
    return 0;
}

int cmdFromOffice(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(
        QStringLiteral("Convert an office document or image to PDF via LibreOffice."));
    p.addPositionalArgument(QStringLiteral("input"),
                            QStringLiteral("Source document (.docx/.odt/.xlsx/…)."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PDF."));
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("from-office needs an input document and an output PDF"));
    if (!Converter::hasOfficeConverter())
        return fail(QStringLiteral("LibreOffice isn't installed; install it for this conversion"));
    QString why;
    if (!Converter::officeToPdf(pos[0], pos[1], &why))
        return fail(why);
    out() << "Wrote " << pos[1] << Qt::endl;
    return 0;
}

int cmdBatch(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(
        QStringLiteral("Run a saved action (a sequence of steps) over one PDF."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("Source PDF."));
    p.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Destination PDF."));
    p.addOption({QStringLiteral("action"),
                 QStringLiteral("Action file (JSON) saved from the Batch wizard."),
                 QStringLiteral("action")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 2)
        return usage(QStringLiteral("batch needs an input and an output PDF"));
    if (!p.isSet(QStringLiteral("action")))
        return usage(QStringLiteral("batch needs --action"));
    QList<BatchRunner::Step> steps;
    QString why;
    if (!BatchRunner::loadAction(p.value(QStringLiteral("action")), &steps, &why))
        return fail(why);
    if (!BatchRunner::runFile(pos[0], pos[1], steps, &why))
        return fail(why);
    out() << "Wrote " << pos[1] << " (" << steps.size() << " steps)" << Qt::endl;
    return 0;
}

int cmdWatch(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(
        QStringLiteral("Watch a folder and run a saved action on each PDF that appears."));
    p.addOption({QStringLiteral("action"), QStringLiteral("Action file (JSON)."),
                 QStringLiteral("file")});
    p.addOption({QStringLiteral("in"), QStringLiteral("Folder to watch."), QStringLiteral("dir")});
    p.addOption({QStringLiteral("out"), QStringLiteral("Where results go. Default: <in>/_out."),
                 QStringLiteral("dir")});
    p.addOption({QStringLiteral("done"),
                 QStringLiteral("Where processed sources move. Default: <in>/_done."),
                 QStringLiteral("dir")});
    p.addOption({QStringLiteral("once"),
                 QStringLiteral("Process what's there now, then exit (don't keep watching).")});
    p.addOption({QStringLiteral("quiet-ms"),
                 QStringLiteral("Skip files touched within this many ms. Default: 1000."),
                 QStringLiteral("ms"), QStringLiteral("1000")});
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    if (!p.isSet(QStringLiteral("action")) || !p.isSet(QStringLiteral("in")))
        return usage(QStringLiteral("watch needs --action and --in"));
    QList<BatchRunner::Step> steps;
    QString why;
    if (!BatchRunner::loadAction(p.value(QStringLiteral("action")), &steps, &why))
        return fail(why);

    const QString inDir = p.value(QStringLiteral("in"));
    const QString outDir = p.value(QStringLiteral("out"));
    const QString doneDir = p.value(QStringLiteral("done"));
    const bool once = p.isSet(QStringLiteral("once"));
    // A single pass processes everything present now, so don't hold files back;
    // the live watcher waits out in-flight writes.
    const int quiet = once ? 0 : p.value(QStringLiteral("quiet-ms")).toInt();

    const auto pass = [&] {
        WatchFolder::Counts c;
        QString w;
        if (!WatchFolder::processPending(inDir, outDir, doneDir, steps, quiet, &c, &w)) {
            err() << "feather-pdf: " << w << Qt::endl;
            return false;
        }
        if (c.processed || c.failed)
            out() << "Processed " << c.processed << ", failed " << c.failed << Qt::endl;
        return true;
    };

    if (once)
        return pass() ? 0 : 1;

    if (!pass()) // initial sweep; a bad folder fails fast before we start watching
        return 1;
    QFileSystemWatcher watcher;
    if (!watcher.addPath(inDir))
        return fail(QStringLiteral("couldn't watch '%1'").arg(inDir));
    // Debounce a burst of changes; also poll periodically in case a change is
    // missed (e.g. files arriving over a network mount).
    QTimer debounce;
    debounce.setSingleShot(true);
    debounce.setInterval(800);
    QObject::connect(&debounce, &QTimer::timeout, [&] { pass(); });
    QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged,
                     [&](const QString&) { debounce.start(); });
    QTimer poll;
    poll.setInterval(5000);
    QObject::connect(&poll, &QTimer::timeout, [&] { pass(); });
    poll.start();
    out() << "Watching " << inDir << " (Ctrl+C to stop)" << Qt::endl;
    return QCoreApplication::exec();
}

int cmdInfo(const QStringList& a) {
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Print a PDF's page count, metadata, and status."));
    p.addPositionalArgument(QStringLiteral("input"), QStringLiteral("PDF to inspect."));
    int rc = 0;
    if (!parseCommand(p, a, &rc))
        return rc;
    const QStringList pos = p.positionalArguments();
    if (pos.size() != 1)
        return usage(QStringLiteral("info needs one PDF"));
    const QString path = pos.first();
    const bool encrypted = PdfEditor::isPasswordProtected(path);
    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(path);
    if (!doc)
        return fail(QStringLiteral("couldn't open '%1'").arg(path));
    out() << "File:      " << path << Qt::endl;
    out() << "Encrypted: " << (encrypted ? "yes" : "no") << Qt::endl;
    if (doc->isLocked()) {
        out() << "Pages:     (locked - password required)" << Qt::endl;
        return 0;
    }
    out() << "Pages:     " << doc->numPages() << Qt::endl;
    const auto field = [&](const char* label, const QString& key) {
        const QString v = doc->info(key);
        if (!v.isEmpty())
            out() << label << v << Qt::endl;
    };
    field("Title:     ", QStringLiteral("Title"));
    field("Author:    ", QStringLiteral("Author"));
    field("Subject:   ", QStringLiteral("Subject"));
    field("Creator:   ", QStringLiteral("Creator"));
    field("Producer:  ", QStringLiteral("Producer"));
    return 0;
}

struct Command {
    const char* name;
    int (*run)(const QStringList&);
    const char* summary;
};

const Command kCommands[] = {
    {"merge", cmdMerge, "Merge several PDFs into one"},
    {"split", cmdSplit, "Split a PDF into several files"},
    {"extract", cmdExtract, "Extract a page range into a new PDF"},
    {"rotate", cmdRotate, "Rotate pages by 90/180/270 degrees"},
    {"encrypt", cmdEncrypt, "Encrypt (AES-256) and set permissions"},
    {"decrypt", cmdDecrypt, "Remove a PDF's password"},
    {"optimize", cmdOptimize, "Shrink a PDF"},
    {"watermark", cmdWatermark, "Stamp a text watermark"},
    {"bates", cmdBates, "Stamp sequential Bates numbers"},
    {"sanitize", cmdSanitize, "Strip metadata, attachments, and scripts"},
    {"ltv", cmdLtv, "Add long-term validation to a signed PDF"},
    {"ocr", cmdOcr, "Add a searchable text layer (OCR)"},
    {"images", cmdImages, "Render pages to image files"},
    {"thumbnail", cmdThumbnail, "Render page one to a PNG thumbnail"},
    {"to-text", cmdToText, "Extract text to a .txt file"},
    {"to-office", cmdToOffice, "Convert to .docx/.odt/.rtf"},
    {"from-images", cmdFromImages, "Build a PDF from images"},
    {"from-office", cmdFromOffice, "Convert a document/image to PDF"},
    {"batch", cmdBatch, "Run a saved action over a PDF"},
    {"watch", cmdWatch, "Watch a folder and run an action on new PDFs"},
    {"info", cmdInfo, "Print page count and metadata"},
};

int printUsage(QTextStream& s) {
    s << "Feather PDF " << QCoreApplication::applicationVersion() << " - command-line interface"
      << Qt::endl
      << Qt::endl
      << "Usage:" << Qt::endl
      << "  feather-pdf                 Launch the graphical app" << Qt::endl
      << "  feather-pdf FILE.pdf        Open FILE in the graphical app" << Qt::endl
      << "  feather-pdf COMMAND [args]  Run a headless operation" << Qt::endl
      << Qt::endl
      << "Commands:" << Qt::endl;
    for (const Command& c : kCommands)
        s << "  " << QString::fromLatin1(c.name).leftJustified(14) << c.summary << Qt::endl;
    s << Qt::endl
      << "Run 'feather-pdf COMMAND --help' for a command's options." << Qt::endl;
    return 0;
}

} // namespace

bool Cli::isCommand(const QString& firstArg) {
    if (firstArg == QLatin1String("-h") || firstArg == QLatin1String("--help")
        || firstArg == QLatin1String("help") || firstArg == QLatin1String("-v")
        || firstArg == QLatin1String("--version") || firstArg == QLatin1String("version"))
        return true;
    for (const Command& c : kCommands)
        if (firstArg == QLatin1String(c.name))
            return true;
    return false;
}

int Cli::run(const QStringList& args) {
    const QString cmd = args.size() > 1 ? args.at(1) : QString();

    if (cmd.isEmpty() || cmd == QLatin1String("-h") || cmd == QLatin1String("--help")
        || cmd == QLatin1String("help"))
        return printUsage(out());

    if (cmd == QLatin1String("-v") || cmd == QLatin1String("--version")
        || cmd == QLatin1String("version")) {
        out() << "Feather PDF " << QCoreApplication::applicationVersion() << Qt::endl;
        return 0;
    }

    for (const Command& c : kCommands)
        if (cmd == QLatin1String(c.name))
            return c.run(args.mid(1)); // pass [command, ...options]

    usage(QStringLiteral("unknown command '%1'. Run 'feather-pdf --help'.").arg(cmd));
    return 2;
}
