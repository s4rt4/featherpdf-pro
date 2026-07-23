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

#include "backends/Ocr.h"

#include "backends/OcrPreprocess.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QMap>
#include <QPainter>
#include <QPdfDocument>
#include <QPointF>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTransform>

#include <vector>

#include <exception>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>

namespace {
QString tesseract() {
    return QStandardPaths::findExecutable(QStringLiteral("tesseract"));
}

std::string num(double v) {
    return QByteArray::number(v, 'f', 2).toStdString();
}

// A page content-stream string literal from OCR text (WinAnsi/Latin-1 bytes).
std::string pdfString(const QString& text) {
    const QByteArray latin1 = text.toLatin1(); // un-encodable → '?'; fine for eng/ind
    std::string out = "(";
    for (char c : latin1) {
        if (c == '(' || c == ')' || c == '\\')
            out += '\\';
        out += c;
    }
    out += ')';
    return out;
}

// Candidate Tesseract language codes for an OSD-reported script, best first.
QStringList scriptCandidates(const QString& script) {
    static const QMap<QString, QStringList> map{
        {QStringLiteral("Latin"), {"eng", "ind", "fra", "deu", "spa", "ita", "por", "nld"}},
        {QStringLiteral("Cyrillic"), {"rus", "ukr", "bul", "srp"}},
        {QStringLiteral("Arabic"), {"ara", "fas", "urd"}},
        {QStringLiteral("Han"), {"chi_sim", "chi_tra"}},
        {QStringLiteral("HanS"), {"chi_sim"}},
        {QStringLiteral("HanT"), {"chi_tra"}},
        {QStringLiteral("Hangul"), {"kor"}},
        {QStringLiteral("Japanese"), {"jpn"}},
        {QStringLiteral("Hiragana"), {"jpn"}},
        {QStringLiteral("Katakana"), {"jpn"}},
        {QStringLiteral("Devanagari"), {"hin", "mar", "san", "nep"}},
        {QStringLiteral("Greek"), {"ell", "grc"}},
        {QStringLiteral("Hebrew"), {"heb"}},
        {QStringLiteral("Thai"), {"tha"}},
    };
    return map.value(script);
}
} // namespace

bool Ocr::isAvailable() {
    return !tesseract().isEmpty();
}

QStringList Ocr::languages() {
    QStringList langs;
    QProcess p;
    p.start(tesseract(), {QStringLiteral("--list-langs")});
    if (!p.waitForFinished(8000))
        return langs;
    const QString out = QString::fromUtf8(p.readAllStandardError()) +
                        QString::fromUtf8(p.readAllStandardOutput());
    for (const QString& line : out.split('\n', Qt::SkipEmptyParts)) {
        const QString l = line.trimmed();
        if (!l.isEmpty() && !l.contains(' ') && l != QStringLiteral("osd"))
            langs << l;
    }
    return langs;
}

QString Ocr::detectLanguage(const QImage& sample, const QString& fallback) {
    const QString tess = tesseract();
    if (tess.isEmpty() || sample.isNull())
        return fallback;

    QTemporaryDir tmp;
    if (!tmp.isValid())
        return fallback;
    const QString imgPath = tmp.filePath(QStringLiteral("osd.png"));
    if (!sample.save(imgPath, "PNG"))
        return fallback;

    QProcess proc;
    proc.start(tess, {imgPath, QStringLiteral("stdout"), QStringLiteral("--psm"),
                      QStringLiteral("0")});
    if (!proc.waitForFinished(30000) || proc.exitStatus() != QProcess::NormalExit)
        return fallback;
    const QString out = QString::fromUtf8(proc.readAllStandardOutput()) +
                        QString::fromUtf8(proc.readAllStandardError());

    QString script;
    for (const QString& line : out.split('\n', Qt::SkipEmptyParts)) {
        if (line.startsWith(QStringLiteral("Script:"))) {
            script = line.section(':', 1).trimmed();
            break;
        }
    }
    if (script.isEmpty())
        return fallback;

    const QStringList installed = languages();
    // Keep the user's pick when it already fits the detected script (e.g. Latin).
    const QStringList candidates = scriptCandidates(script);
    if (candidates.contains(fallback))
        return fallback;
    for (const QString& code : candidates) {
        if (installed.contains(code))
            return code;
    }
    return fallback;
}

bool Ocr::addTextLayer(const QString& inputPath, const QString& outputPath,
                       const QString& language, const Options& options, QString* error) {
    const QString tess = tesseract();
    if (tess.isEmpty()) {
        if (error)
            *error = QStringLiteral("Tesseract isn't installed, so text recognition isn't "
                                    "available.");
        return false;
    }

    QPdfDocument doc;
    if (doc.load(inputPath) != QPdfDocument::Error::None) {
        if (error)
            *error = QStringLiteral("The document couldn't be opened for recognition.");
        return false;
    }

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        if (error)
            *error = QStringLiteral("Couldn't create a temporary working directory.");
        return false;
    }

    const QString target = outputPath + QStringLiteral(".feather-tmp");
    constexpr double kDpi = 300.0;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());
        QPDFPageDocumentHelper dh(qpdf);
        dh.pushInheritedAttributesToPage();
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        const int n = std::min<int>(doc.pageCount(), static_cast<int>(pages.size()));

        QString lang = language;
        bool langResolved = !options.autoLanguage;

        for (int i = 0; i < n; ++i) {
            QSizeF pt = doc.pagePointSize(i);
            if (pt.width() <= 0 || pt.height() <= 0)
                continue;
            const QSize px(qRound(pt.width() * kDpi / 72.0), qRound(pt.height() * kDpi / 72.0));

            // Render the page (native orientation) onto white for clean OCR.
            QImage flat(px, QImage::Format_RGB888);
            flat.fill(Qt::white);
            const QImage rendered = doc.render(i, px);
            if (!rendered.isNull()) {
                QPainter pp(&flat);
                pp.drawImage(0, 0, rendered);
            }

            // In-process clean-up before Tesseract. Deskew may rotate (and resize)
            // the image, so remember the flat->processed transform to map word
            // boxes back onto the original page coordinates afterwards.
            QImage proc = flat;
            QTransform flatToProc; // identity unless deskew rotates
            if (options.deskew) {
                const double angle = OcrPreprocess::estimateSkew(proc);
                proc = OcrPreprocess::rotated(proc, angle, &flatToProc);
            }
            if (options.binarize)
                proc = OcrPreprocess::binarize(proc);
            else if (options.despeckle)
                proc = OcrPreprocess::grayscale(proc);
            if (options.despeckle)
                proc = OcrPreprocess::despeckle(proc);
            const QTransform procToFlat = flatToProc.inverted();

            const QString imgPath = tmp.filePath(QStringLiteral("p%1.png").arg(i));
            if (!proc.save(imgPath, "PNG"))
                continue;

            // Auto language: detect the script once, from the first cleaned page.
            if (!langResolved) {
                lang = detectLanguage(proc, language);
                langResolved = true;
            }

            QProcess proc2;
            proc2.start(tess, {imgPath, QStringLiteral("stdout"), QStringLiteral("-l"), lang,
                               QStringLiteral("tsv")});
            if (!proc2.waitForFinished(180000) || proc2.exitStatus() != QProcess::NormalExit)
                continue;
            const QString tsv = QString::fromUtf8(proc2.readAllStandardOutput());

            const double sx = pt.width() / px.width();
            const double sy = pt.height() / px.height();
            std::string content = "BT\n3 Tr\n"; // invisible text rendering mode
            bool any = false;
            const QStringList lines = tsv.split('\n');
            for (int li = 1; li < lines.size(); ++li) { // skip the header row
                const QStringList c = lines[li].split('\t');
                if (c.size() < 12 || c[0] != QStringLiteral("5")) // level 5 = word
                    continue;
                if (c[10].toDouble() < 0) // confidence
                    continue;
                const QString word = c[11];
                if (word.trimmed().isEmpty())
                    continue;
                const double left = c[6].toDouble();
                const double top = c[7].toDouble();
                const double h = c[9].toDouble();
                const double fs = h * sy;
                if (fs <= 0)
                    continue;
                // Box baseline (bottom-left) in processed-image pixels, mapped back
                // to the un-deskewed page image, then to PDF points.
                const QPointF fp = procToFlat.map(QPointF(left, top + h));
                const double x = fp.x() * sx;
                const double y = pt.height() - fp.y() * sy; // baseline near box bottom
                content += "/FtOCR " + num(fs) + " Tf 1 0 0 1 " + num(x) + " " + num(y) +
                           " Tm " + pdfString(word) + " Tj\n";
                any = true;
            }
            content += "ET\n";
            if (!any)
                continue;

            // Ensure the page resources carry our Helvetica font.
            QPDFObjectHandle page = pages[i].getObjectHandle();
            QPDFObjectHandle res = page.getKey("/Resources");
            if (!res.isDictionary()) {
                res = QPDFObjectHandle::newDictionary();
                page.replaceKey("/Resources", res);
            }
            QPDFObjectHandle fonts = res.getKey("/Font");
            if (!fonts.isDictionary()) {
                fonts = QPDFObjectHandle::newDictionary();
                res.replaceKey("/Font", fonts);
            }
            if (!fonts.hasKey("/FtOCR")) {
                QPDFObjectHandle font = QPDFObjectHandle::newDictionary();
                font.replaceKey("/Type", QPDFObjectHandle::newName("/Font"));
                font.replaceKey("/Subtype", QPDFObjectHandle::newName("/Type1"));
                font.replaceKey("/BaseFont", QPDFObjectHandle::newName("/Helvetica"));
                font.replaceKey("/Encoding", QPDFObjectHandle::newName("/WinAnsiEncoding"));
                fonts.replaceKey("/FtOCR", qpdf.makeIndirectObject(font));
            }

            pages[i].addPageContents(QPDFObjectHandle::newStream(&qpdf, content), /*first=*/false);
        }

        QPDFWriter writer(qpdf, target.toLocal8Bit().constData());
        writer.write();
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        QFile::remove(target);
        return false;
    }

    QFile::remove(outputPath);
    if (!QFile::rename(target, outputPath)) {
        if (error)
            *error = QStringLiteral("The recognized file couldn't be written.");
        QFile::remove(target);
        return false;
    }
    return true;
}
