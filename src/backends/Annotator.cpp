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

#include "backends/Annotator.h"

#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <exception>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>

// We author highlights with QPDF rather than Poppler so we can attach an explicit
// appearance stream (/AP). PDFium (our QtPdf viewer) renders an annotation's /AP
// directly; without one it falls back to its own quad interpretation, which draws
// text-markup highlights incorrectly. Building the /AP ourselves makes the result
// render identically everywhere.

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

// Escape a string for a PDF literal-string operand in a content stream (Latin-1).
std::string litStr(const QString& s) {
    std::string o = "(";
    for (char c : s.toLatin1()) {
        if (c == '(' || c == ')' || c == '\\')
            o += '\\';
        o += c;
    }
    o += ")";
    return o;
}
} // namespace

namespace {
// Append an annotation to a page's /Annots, creating the array if absent.
void appendAnnot(QPDFObjectHandle& page, QPDFObjectHandle annot, QPDF& qpdf) {
    QPDFObjectHandle annots = page.getKey("/Annots");
    if (!annots.isArray())
        annots = QPDFObjectHandle::newArray();
    annots.appendItem(qpdf.makeIndirectObject(annot));
    page.replaceKey("/Annots", annots);
}
} // namespace

bool Annotator::saveAnnotations(const QString& inputPath, const QString& outputPath,
                                const QList<Highlight>& highlights, const QList<Note>& notes,
                                const QList<Ink>& inks, const QList<Shape>& shapes, QString* error) {
    if (highlights.isEmpty() && notes.isEmpty() && inks.isEmpty() && shapes.isEmpty()) {
        if (error)
            *error = QStringLiteral("Nothing to save.");
        return false;
    }

    const QString target = outputPath + QStringLiteral(".feather-tmp");

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        QPDFPageDocumentHelper dh(qpdf);
        dh.pushInheritedAttributesToPage(); // ensure each page carries its own MediaBox
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        const int n = static_cast<int>(pages.size());

        for (const Highlight& hl : highlights) {
            if (hl.page < 0 || hl.page >= n)
                continue;
            QPDFObjectHandle page = pages[hl.page].getObjectHandle();

            // Page box → absolute PDF coordinates (origin bottom-left).
            QPDFObjectHandle mb = page.getKey("/MediaBox");
            const double llx = mb.getArrayItem(0).getNumericValue();
            const double lly = mb.getArrayItem(1).getNumericValue();
            const double urx = mb.getArrayItem(2).getNumericValue();
            const double ury = mb.getArrayItem(3).getNumericValue();
            const double w = urx - llx;
            const double h = ury - lly;

            const double x0 = llx + hl.rect.left() * w;
            const double x1 = llx + hl.rect.right() * w;
            const double yTop = ury - hl.rect.top() * h;    // normalized top → high y
            const double yBot = ury - hl.rect.bottom() * h; // normalized bottom → low y

            const double r = hl.color.redF();
            const double g = hl.color.greenF();
            const double b = hl.color.blueF();
            constexpr double opacity = 0.4;

            // Appearance: a Multiply-blended filled rectangle (highlighter look).
            QPDFObjectHandle form = QPDFObjectHandle::newStream(&qpdf);
            QPDFObjectHandle fd = form.getDict();
            fd.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
            fd.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
            fd.replaceKey("/BBox", numberArray({x0, yBot, x1, yTop}));
            QPDFObjectHandle gs = QPDFObjectHandle::newDictionary();
            gs.replaceKey("/BM", QPDFObjectHandle::newName("/Multiply"));
            QPDFObjectHandle egs = QPDFObjectHandle::newDictionary();
            egs.replaceKey("/GS0", gs);
            QPDFObjectHandle res = QPDFObjectHandle::newDictionary();
            res.replaceKey("/ExtGState", egs);
            fd.replaceKey("/Resources", res);
            const std::string content = "/GS0 gs\n" + num(r) + " " + num(g) + " " + num(b) +
                                        " rg\n" + num(x0) + " " + num(yBot) + " " + num(x1 - x0) +
                                        " " + num(yTop - yBot) + " re f\n";
            form.replaceStreamData(content, QPDFObjectHandle::newNull(),
                                   QPDFObjectHandle::newNull());

            QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
            ap.replaceKey("/N", form);

            QPDFObjectHandle annot = QPDFObjectHandle::newDictionary();
            annot.replaceKey("/Type", QPDFObjectHandle::newName("/Annot"));
            annot.replaceKey("/Subtype", QPDFObjectHandle::newName("/Highlight"));
            annot.replaceKey("/Rect", numberArray({x0, yBot, x1, yTop}));
            // QuadPoints (text-markup convention: upper-left, upper-right,
            // lower-left, lower-right).
            annot.replaceKey("/QuadPoints",
                             numberArray({x0, yTop, x1, yTop, x0, yBot, x1, yBot}));
            annot.replaceKey("/C", numberArray({r, g, b}));
            annot.replaceKey("/CA", QPDFObjectHandle::newReal(num(opacity)));
            annot.replaceKey("/F", QPDFObjectHandle::newInteger(4)); // Print
            annot.replaceKey("/AP", ap);

            appendAnnot(page, annot, qpdf);
        }

        for (const Note& note : notes) {
            if (note.page < 0 || note.page >= n)
                continue;
            QPDFObjectHandle page = pages[note.page].getObjectHandle();
            QPDFObjectHandle mb = page.getKey("/MediaBox");
            const double llx = mb.getArrayItem(0).getNumericValue();
            const double lly = mb.getArrayItem(1).getNumericValue();
            const double urx = mb.getArrayItem(2).getNumericValue();
            const double ury = mb.getArrayItem(3).getNumericValue();
            const double w = urx - llx;
            const double h = ury - lly;

            constexpr double s = 20.0; // icon size in points
            const double x = llx + note.pos.x() * w;
            const double yTop = ury - note.pos.y() * h;
            const double y = yTop - s; // icon grows downward from the anchor

            const double r = note.color.redF();
            const double g = note.color.greenF();
            const double b = note.color.blueF();

            // Appearance: a little note card with three "text" lines.
            QPDFObjectHandle form = QPDFObjectHandle::newStream(&qpdf);
            QPDFObjectHandle fd = form.getDict();
            fd.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
            fd.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
            fd.replaceKey("/BBox", numberArray({x, y, x + s, y + s}));
            const std::string content =
                num(r) + " " + num(g) + " " + num(b) + " rg " + num(x) + " " + num(y) + " " +
                num(s) + " " + num(s) + " re f " + "0.25 0.25 0.25 RG 1.4 w " + num(x + 4) + " " +
                num(y + 14) + " m " + num(x + 16) + " " + num(y + 14) + " l S " + num(x + 4) + " " +
                num(y + 10) + " m " + num(x + 16) + " " + num(y + 10) + " l S " + num(x + 4) + " " +
                num(y + 6) + " m " + num(x + 12) + " " + num(y + 6) + " l S";
            form.replaceStreamData(content, QPDFObjectHandle::newNull(),
                                   QPDFObjectHandle::newNull());
            QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
            ap.replaceKey("/N", form);

            QPDFObjectHandle annot = QPDFObjectHandle::newDictionary();
            annot.replaceKey("/Type", QPDFObjectHandle::newName("/Annot"));
            annot.replaceKey("/Subtype", QPDFObjectHandle::newName("/Text"));
            annot.replaceKey("/Rect", numberArray({x, y, x + s, y + s}));
            annot.replaceKey("/Name", QPDFObjectHandle::newName("/Comment"));
            annot.replaceKey("/Contents", QPDFObjectHandle::newUnicodeString(note.text.toStdString()));
            annot.replaceKey("/C", numberArray({r, g, b}));
            annot.replaceKey("/F", QPDFObjectHandle::newInteger(4));
            annot.replaceKey("/AP", ap);

            appendAnnot(page, annot, qpdf);
        }

        for (const Ink& ink : inks) {
            if (ink.page < 0 || ink.page >= n || ink.strokes.isEmpty())
                continue;
            QPDFObjectHandle page = pages[ink.page].getObjectHandle();
            QPDFObjectHandle mb = page.getKey("/MediaBox");
            const double llx = mb.getArrayItem(0).getNumericValue();
            const double lly = mb.getArrayItem(1).getNumericValue();
            const double urx = mb.getArrayItem(2).getNumericValue();
            const double ury = mb.getArrayItem(3).getNumericValue();
            const double pw = urx - llx;
            const double ph = ury - lly;

            const double red = ink.color.redF();
            const double grn = ink.color.greenF();
            const double blu = ink.color.blueF();
            constexpr double penW = 2.0;

            // Map normalized strokes to PDF coordinates, tracking the bounds.
            QList<QList<QPointF>> pdfStrokes;
            double minX = urx, minY = ury, maxX = llx, maxY = lly;
            for (const QPolygonF& s : ink.strokes) {
                QList<QPointF> ps;
                for (const QPointF& nrm : s) {
                    const double x = llx + nrm.x() * pw;
                    const double y = ury - nrm.y() * ph;
                    ps.append(QPointF(x, y));
                    minX = std::min(minX, x);
                    minY = std::min(minY, y);
                    maxX = std::max(maxX, x);
                    maxY = std::max(maxY, y);
                }
                if (ps.size() >= 2)
                    pdfStrokes.append(ps);
            }
            if (pdfStrokes.isEmpty())
                continue;
            const double pad = penW + 1.0;
            minX -= pad;
            minY -= pad;
            maxX += pad;
            maxY += pad;

            // Appearance stream: stroke each path with round caps/joins.
            std::string draw = num(red) + " " + num(grn) + " " + num(blu) + " RG " + num(penW) +
                               " w 1 J 1 j ";
            QPDFObjectHandle inkList = QPDFObjectHandle::newArray();
            for (const QList<QPointF>& ps : pdfStrokes) {
                QPDFObjectHandle coords = QPDFObjectHandle::newArray();
                for (int i = 0; i < ps.size(); ++i) {
                    coords.appendItem(QPDFObjectHandle::newReal(num(ps[i].x())));
                    coords.appendItem(QPDFObjectHandle::newReal(num(ps[i].y())));
                    draw += num(ps[i].x()) + " " + num(ps[i].y()) + (i == 0 ? " m " : " l ");
                }
                draw += "S ";
                inkList.appendItem(coords);
            }

            QPDFObjectHandle form = QPDFObjectHandle::newStream(&qpdf);
            QPDFObjectHandle fd = form.getDict();
            fd.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
            fd.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
            fd.replaceKey("/BBox", numberArray({minX, minY, maxX, maxY}));
            form.replaceStreamData(draw, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
            QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
            ap.replaceKey("/N", form);

            QPDFObjectHandle annot = QPDFObjectHandle::newDictionary();
            annot.replaceKey("/Type", QPDFObjectHandle::newName("/Annot"));
            annot.replaceKey("/Subtype", QPDFObjectHandle::newName("/Ink"));
            annot.replaceKey("/Rect", numberArray({minX, minY, maxX, maxY}));
            annot.replaceKey("/InkList", inkList);
            annot.replaceKey("/C", numberArray({red, grn, blu}));
            annot.replaceKey("/F", QPDFObjectHandle::newInteger(4));
            annot.replaceKey("/AP", ap);

            appendAnnot(page, annot, qpdf);
        }

        for (const Shape& sh : shapes) {
            if (sh.page < 0 || sh.page >= n)
                continue;
            QPDFObjectHandle page = pages[sh.page].getObjectHandle();
            QPDFObjectHandle mb = page.getKey("/MediaBox");
            const double llx = mb.getArrayItem(0).getNumericValue();
            const double ury = mb.getArrayItem(3).getNumericValue();
            const double w = mb.getArrayItem(2).getNumericValue() - llx;
            const double h = ury - mb.getArrayItem(1).getNumericValue();

            const double r = sh.color.redF(), g = sh.color.greenF(), b = sh.color.blueF();
            constexpr double penW = 1.5;

            // Line / Arrow: two endpoints, optionally with an arrowhead at the end.
            if (sh.kind == Shape::Kind::Line || sh.kind == Shape::Kind::Arrow) {
                constexpr double len_pad = 12.0; // BBox padding for the arrowhead/pen
                const double ax = llx + sh.a.x() * w, ay = ury - sh.a.y() * h;
                const double bx = llx + sh.b.x() * w, by = ury - sh.b.y() * h;
                std::string draw =
                    num(r) + " " + num(g) + " " + num(b) + " RG " + num(penW) + " w 1 J " +
                    num(ax) + " " + num(ay) + " m " + num(bx) + " " + num(by) + " l S";
                if (sh.kind == Shape::Kind::Arrow) {
                    const double ang = std::atan2(ay - by, ax - bx);
                    constexpr double len = 10.0, spread = 0.45;
                    const double h1x = bx + std::cos(ang + spread) * len;
                    const double h1y = by + std::sin(ang + spread) * len;
                    const double h2x = bx + std::cos(ang - spread) * len;
                    const double h2y = by + std::sin(ang - spread) * len;
                    draw += " " + num(r) + " " + num(g) + " " + num(b) + " rg " + num(bx) + " " +
                            num(by) + " m " + num(h1x) + " " + num(h1y) + " l " + num(h2x) + " " +
                            num(h2y) + " l f";
                }
                const double minX = std::min(ax, bx) - len_pad, minY = std::min(ay, by) - len_pad;
                const double maxX = std::max(ax, bx) + len_pad, maxY = std::max(ay, by) + len_pad;
                QPDFObjectHandle form = QPDFObjectHandle::newStream(&qpdf);
                QPDFObjectHandle fd = form.getDict();
                fd.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
                fd.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
                fd.replaceKey("/BBox", numberArray({minX, minY, maxX, maxY}));
                form.replaceStreamData(draw, QPDFObjectHandle::newNull(),
                                       QPDFObjectHandle::newNull());
                QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
                ap.replaceKey("/N", form);
                QPDFObjectHandle annot = QPDFObjectHandle::newDictionary();
                annot.replaceKey("/Type", QPDFObjectHandle::newName("/Annot"));
                annot.replaceKey("/Subtype", QPDFObjectHandle::newName("/Line"));
                annot.replaceKey("/Rect", numberArray({minX, minY, maxX, maxY}));
                annot.replaceKey("/L", numberArray({ax, ay, bx, by}));
                if (sh.kind == Shape::Kind::Arrow) {
                    QPDFObjectHandle le = QPDFObjectHandle::newArray();
                    le.appendItem(QPDFObjectHandle::newName("/None"));
                    le.appendItem(QPDFObjectHandle::newName("/ClosedArrow"));
                    annot.replaceKey("/LE", le);
                }
                annot.replaceKey("/C", numberArray({r, g, b}));
                annot.replaceKey("/F", QPDFObjectHandle::newInteger(4));
                annot.replaceKey("/AP", ap);
                appendAnnot(page, annot, qpdf);
                continue;
            }

            const double x0 = llx + sh.rect.left() * w;
            const double x1 = llx + sh.rect.right() * w;
            const double yTop = ury - sh.rect.top() * h;
            const double yBot = ury - sh.rect.bottom() * h;

            // Text box: a free-text annotation with a border and the typed text.
            if (sh.kind == Shape::Kind::TextBox) {
                const double size = std::max(7.0, std::min(12.0, (yTop - yBot) - 4));
                QPDFObjectHandle helv = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
                helv.replaceKey("/Type", QPDFObjectHandle::newName("/Font"));
                helv.replaceKey("/Subtype", QPDFObjectHandle::newName("/Type1"));
                helv.replaceKey("/BaseFont", QPDFObjectHandle::newName("/Helvetica"));
                QPDFObjectHandle fonts = QPDFObjectHandle::newDictionary();
                fonts.replaceKey("/Helv", helv);
                QPDFObjectHandle res = QPDFObjectHandle::newDictionary();
                res.replaceKey("/Font", fonts);
                const std::string da = "/Helv " + num(size) + " Tf " + num(r) + " " + num(g) + " " +
                                       num(b) + " rg";
                std::string draw = num(r) + " " + num(g) + " " + num(b) + " RG " + num(penW) +
                                   " w " + num(x0 + 0.75) + " " + num(yBot + 0.75) + " " +
                                   num(x1 - x0 - penW) + " " + num(yTop - yBot - penW) + " re S\n" +
                                   "BT " + da + " " + num(x0 + 3) + " " + num(yTop - size - 2) +
                                   " Td " + litStr(sh.text) + " Tj ET";
                QPDFObjectHandle form = QPDFObjectHandle::newStream(&qpdf);
                QPDFObjectHandle fd = form.getDict();
                fd.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
                fd.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
                fd.replaceKey("/BBox", numberArray({x0, yBot, x1, yTop}));
                fd.replaceKey("/Resources", res);
                form.replaceStreamData(draw, QPDFObjectHandle::newNull(),
                                       QPDFObjectHandle::newNull());
                QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
                ap.replaceKey("/N", form);
                QPDFObjectHandle annot = QPDFObjectHandle::newDictionary();
                annot.replaceKey("/Type", QPDFObjectHandle::newName("/Annot"));
                annot.replaceKey("/Subtype", QPDFObjectHandle::newName("/FreeText"));
                annot.replaceKey("/Rect", numberArray({x0, yBot, x1, yTop}));
                annot.replaceKey("/Contents", QPDFObjectHandle::newUnicodeString(sh.text.toStdString()));
                annot.replaceKey("/DA", QPDFObjectHandle::newString(da));
                annot.replaceKey("/C", numberArray({r, g, b}));
                annot.replaceKey("/F", QPDFObjectHandle::newInteger(4));
                annot.replaceKey("/AP", ap);
                appendAnnot(page, annot, qpdf);
                continue;
            }

            const char* subtype = "/Square";
            std::string draw =
                num(r) + " " + num(g) + " " + num(b) + " RG " + num(penW) + " w 1 J ";
            if (sh.kind == Shape::Kind::Underline) {
                subtype = "/Underline";
                const double y = yBot + penW; // a touch above the bottom edge
                draw += num(x0) + " " + num(y) + " m " + num(x1) + " " + num(y) + " l S";
            } else if (sh.kind == Shape::Kind::StrikeOut) {
                subtype = "/StrikeOut";
                const double y = (yTop + yBot) / 2;
                draw += num(x0) + " " + num(y) + " m " + num(x1) + " " + num(y) + " l S";
            } else { // Rectangle → /Square
                const double in = penW / 2;
                draw += num(x0 + in) + " " + num(yBot + in) + " " + num(x1 - x0 - penW) + " " +
                        num(yTop - yBot - penW) + " re S";
            }

            QPDFObjectHandle form = QPDFObjectHandle::newStream(&qpdf);
            QPDFObjectHandle fd = form.getDict();
            fd.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
            fd.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
            fd.replaceKey("/BBox", numberArray({x0, yBot, x1, yTop}));
            form.replaceStreamData(draw, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
            QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
            ap.replaceKey("/N", form);

            QPDFObjectHandle annot = QPDFObjectHandle::newDictionary();
            annot.replaceKey("/Type", QPDFObjectHandle::newName("/Annot"));
            annot.replaceKey("/Subtype", QPDFObjectHandle::newName(subtype));
            annot.replaceKey("/Rect", numberArray({x0, yBot, x1, yTop}));
            if (sh.kind != Shape::Kind::Rectangle) // text markup carries QuadPoints
                annot.replaceKey("/QuadPoints",
                                 numberArray({x0, yTop, x1, yTop, x0, yBot, x1, yBot}));
            annot.replaceKey("/C", numberArray({r, g, b}));
            annot.replaceKey("/F", QPDFObjectHandle::newInteger(4));
            annot.replaceKey("/AP", ap);

            appendAnnot(page, annot, qpdf);
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
            *error = QStringLiteral("The annotated file couldn't replace the original.");
        QFile::remove(target);
        return false;
    }
    return true;
}
