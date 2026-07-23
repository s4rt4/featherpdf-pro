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

#include "backends/StampLibrary.h"

#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <exception>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

// We author the stamp with QPDF (not Poppler) so we can attach an explicit
// appearance stream (/AP). PDFium (our QtPdf viewer) renders an annotation's /AP
// directly, so building it ourselves makes the result render identically
// everywhere — mirroring how Annotator authors its highlights and shapes.

namespace {
std::string num(double v) {
    return QByteArray::number(v, 'f', 2).toStdString();
}

QPDFObjectHandle numberArray(std::initializer_list<double> vals) {
    QPDFObjectHandle a = QPDFObjectHandle::newArray();
    for (double v : vals)
        a.appendItem(QPDFObjectHandle::newReal(num(v)));
    return a;
}

// Escape a string for a PDF literal-string operand drawn with a WinAnsi-encoded
// font. The captions can carry an em-dash (U+2014), which WinAnsiEncoding maps to
// byte 0x97; plain Latin-1 has no such glyph, so we map the dashes explicitly.
std::string litStr(const QString& s) {
    std::string o = "(";
    for (const QChar& qc : s) {
        const ushort u = qc.unicode();
        unsigned char c;
        if (u == 0x2014) // em dash
            c = 0x97;
        else if (u == 0x2013) // en dash
            c = 0x96;
        else if (u < 0x100)
            c = static_cast<unsigned char>(u);
        else
            c = '?';
        if (c == '(' || c == ')' || c == '\\')
            o += '\\';
        o += static_cast<char>(c);
    }
    o += ")";
    return o;
}

// The page's MediaBox as [x0,y0,x1,y1], normalized so x0<x1, y0<y1.
bool mediaBox(QPDFObjectHandle page, double out[4]) {
    QPDFObjectHandle b = page.getKey("/MediaBox");
    if (!b.isArray() || b.getArrayNItems() != 4)
        return false;
    double v[4];
    for (int i = 0; i < 4; ++i) {
        QPDFObjectHandle e = b.getArrayItem(i);
        if (!e.isNumber())
            return false;
        v[i] = e.getNumericValue();
    }
    out[0] = std::min(v[0], v[2]);
    out[1] = std::min(v[1], v[3]);
    out[2] = std::max(v[0], v[2]);
    out[3] = std::max(v[1], v[3]);
    return true;
}

int pageRotate(QPDFObjectHandle page) {
    QPDFObjectHandle r = page.getKey("/Rotate");
    const int v = r.isInteger() ? r.getIntValueAsInt() : 0;
    return ((v % 360) + 360) % 360;
}

// Map a displayed-page (top-left origin) fraction rect to the page's unrotated
// fractions, for a clockwise display rotation `rot`.
QRectF toUnrotatedFraction(const QRectF& r, int rot) {
    const auto m = [&](double u, double v) -> QPointF {
        switch (rot) {
        case 90: return {v, 1.0 - u};
        case 180: return {1.0 - u, 1.0 - v};
        case 270: return {1.0 - v, u};
        default: return {u, v};
        }
    };
    const QPointF a = m(r.left(), r.top());
    const QPointF b = m(r.right(), r.bottom());
    return QRectF(QPointF(std::min(a.x(), b.x()), std::min(a.y(), b.y())),
                  QPointF(std::max(a.x(), b.x()), std::max(a.y(), b.y())));
}

// Append a rounded-rectangle path (corner radius `r`) to a content stream.
std::string roundedRect(double x, double y, double w, double h, double r) {
    constexpr double k = 0.5523; // circle → bezier magic constant
    r = std::min(r, std::min(w, h) / 2.0);
    const double x1 = x + w, y1 = y + h;
    std::string p;
    p += num(x + r) + " " + num(y) + " m ";
    p += num(x1 - r) + " " + num(y) + " l ";
    p += num(x1 - r + r * k) + " " + num(y) + " " + num(x1) + " " + num(y + r - r * k) + " " +
         num(x1) + " " + num(y + r) + " c ";
    p += num(x1) + " " + num(y1 - r) + " l ";
    p += num(x1) + " " + num(y1 - r + r * k) + " " + num(x1 - r + r * k) + " " + num(y1) + " " +
         num(x1 - r) + " " + num(y1) + " c ";
    p += num(x + r) + " " + num(y1) + " l ";
    p += num(x + r - r * k) + " " + num(y1) + " " + num(x) + " " + num(y1 - r + r * k) + " " +
         num(x) + " " + num(y1 - r) + " c ";
    p += num(x) + " " + num(y + r) + " l ";
    p += num(x) + " " + num(y + r - r * k) + " " + num(x + r - r * k) + " " + num(y) + " " +
         num(x + r) + " " + num(y) + " c ";
    p += "h ";
    return p;
}

const char* presetLabel(StampLibrary::Preset preset) {
    switch (preset) {
    case StampLibrary::Preset::Approved: return "APPROVED";
    case StampLibrary::Preset::Draft: return "DRAFT";
    case StampLibrary::Preset::Confidential: return "CONFIDENTIAL";
    case StampLibrary::Preset::ForReview: return "FOR REVIEW";
    }
    return "APPROVED";
}
} // namespace

QString StampLibrary::caption(Preset preset, const QString& date, const QString& name) {
    QString c = QString::fromLatin1(presetLabel(preset));
    if (!date.trimmed().isEmpty())
        c += QStringLiteral(" - ") + date.trimmed();
    if (!name.trimmed().isEmpty())
        c += QStringLiteral(" - ") + name.trimmed();
    return c;
}

QColor StampLibrary::color(Preset preset) {
    switch (preset) {
    case Preset::Approved: return QColor(0x1f, 0x9d, 0x55);    // green
    case Preset::Draft: return QColor(0xd9, 0x8a, 0x0b);       // amber
    case Preset::Confidential: return QColor(0xc8, 0x2b, 0x2b); // red
    case Preset::ForReview: return QColor(0x21, 0x6e, 0xc8);   // blue
    }
    return QColor(0x1f, 0x9d, 0x55);
}

bool StampLibrary::addStamp(const QString& inputPath, const QString& outputPath, int page,
                            const QRectF& normRect, Preset preset, const QString& date,
                            const QString& name, QString* error) {
    const QString text = caption(preset, date, name);
    const QString target = outputPath + QStringLiteral(".feather-tmp");
    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString writePath = inPlace ? target : outputPath;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        QPDFPageDocumentHelper dh(qpdf);
        dh.pushInheritedAttributesToPage(); // ensure each page carries its own MediaBox
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        if (page < 0 || page >= int(pages.size())) {
            if (error)
                *error = QStringLiteral("That page is no longer in the document.");
            return false;
        }
        QPDFObjectHandle pageObj = pages[size_t(page)].getObjectHandle();
        double mb[4];
        if (!mediaBox(pageObj, mb)) {
            if (error)
                *error = QStringLiteral("The page has no MediaBox.");
            return false;
        }

        // The drawn rect is in displayed-page fractions; undo any display rotation
        // so it lands on the page's natural orientation, then map to PDF points.
        const QRectF nr = toUnrotatedFraction(normRect, pageRotate(pageObj));
        const double W = mb[2] - mb[0], H = mb[3] - mb[1];
        const double x0 = mb[0] + nr.left() * W;
        const double x1 = mb[0] + nr.right() * W;
        const double yBot = mb[3] - nr.bottom() * H; // top-left fractions → PDF y
        const double yTop = mb[3] - nr.top() * H;
        const double w = x1 - x0, h = yTop - yBot;
        if (w < 2.0 || h < 2.0) {
            if (error)
                *error = QStringLiteral("The stamp area is too small.");
            return false;
        }

        const double r = color(preset).redF();
        const double g = color(preset).greenF();
        const double b = color(preset).blueF();
        const double penW = std::max(1.5, std::min(3.0, h * 0.06));
        const double pad = std::max(4.0, h * 0.16);
        const double radius = std::min(h * 0.22, 10.0);

        // Size the caption to fit both the box height and width. Helvetica-Bold
        // averages roughly 0.6 em per glyph, so width bounds the font as well.
        const int len = std::max(1, int(text.length()));
        const double sizeH = (h - 2 * pad);
        const double sizeW = (w - 2 * pad) / (0.6 * len);
        const double size = std::max(5.0, std::min({sizeH, sizeW, 40.0}));
        const double textW = 0.6 * size * len;
        const double tx = x0 + (w - textW) / 2.0;
        const double ty = yBot + (h - size) / 2.0 + size * 0.22; // baseline, vertically centred

        // Bold caption font for the appearance stream's /Resources.
        QPDFObjectHandle font = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
        font.replaceKey("/Type", QPDFObjectHandle::newName("/Font"));
        font.replaceKey("/Subtype", QPDFObjectHandle::newName("/Type1"));
        font.replaceKey("/BaseFont", QPDFObjectHandle::newName("/Helvetica-Bold"));
        font.replaceKey("/Encoding", QPDFObjectHandle::newName("/WinAnsiEncoding"));
        QPDFObjectHandle fonts = QPDFObjectHandle::newDictionary();
        fonts.replaceKey("/HelvB", font);
        QPDFObjectHandle res = QPDFObjectHandle::newDictionary();
        res.replaceKey("/Font", fonts);

        const std::string rgb = num(r) + " " + num(g) + " " + num(b);
        // Rounded bordered rectangle (stroke) + centred bold caption.
        const std::string draw =
            rgb + " RG " + num(penW) + " w " +
            roundedRect(x0 + penW, yBot + penW, w - 2 * penW, h - 2 * penW, radius) + "S\n" +
            "BT /HelvB " + num(size) + " Tf " + rgb + " rg " + num(tx) + " " + num(ty) + " Td " +
            litStr(text) + " Tj ET";

        QPDFObjectHandle form = QPDFObjectHandle::newStream(&qpdf);
        QPDFObjectHandle fd = form.getDict();
        fd.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
        fd.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
        fd.replaceKey("/BBox", numberArray({x0, yBot, x1, yTop}));
        fd.replaceKey("/Resources", res);
        form.replaceStreamData(draw, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());

        QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
        ap.replaceKey("/N", form);

        QPDFObjectHandle annot = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
        annot.replaceKey("/Type", QPDFObjectHandle::newName("/Annot"));
        annot.replaceKey("/Subtype", QPDFObjectHandle::newName("/Stamp"));
        annot.replaceKey("/Name", QPDFObjectHandle::newName("/Draft"));
        annot.replaceKey("/Rect", numberArray({x0, yBot, x1, yTop}));
        annot.replaceKey("/Contents", QPDFObjectHandle::newUnicodeString(text.toStdString()));
        annot.replaceKey("/C", numberArray({r, g, b}));
        annot.replaceKey("/F", QPDFObjectHandle::newInteger(4)); // Print
        annot.replaceKey("/AP", ap);

        QPDFObjectHandle annots = pageObj.getKey("/Annots");
        if (!annots.isArray()) {
            annots = QPDFObjectHandle::newArray();
            pageObj.replaceKey("/Annots", annots);
        }
        annots.appendItem(annot);

        QPDFWriter writer(qpdf, writePath.toLocal8Bit().constData());
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
                *error = QStringLiteral("The stamped file couldn't replace the original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}
