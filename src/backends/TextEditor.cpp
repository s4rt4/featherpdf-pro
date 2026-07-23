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

#include "backends/TextEditor.h"

#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <exception>
#include <functional>
#include <vector>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

namespace {
std::string num(double v) {
    return QByteArray::number(v, 'f', 2).toStdString();
}

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

// Read a 4-number box into [x0,y0,x1,y1]; false if not a 4-array.
bool box4(QPDFObjectHandle b, double out[4]) {
    if (!b.isArray() || b.getArrayNItems() != 4)
        return false;
    for (int i = 0; i < 4; ++i)
        out[i] = b.getArrayItem(i).getNumericValue();
    return true;
}

// Build (and attach) a /FreeText appearance: a border plus the text, drawn in
// the annotation's own (absolute page) coordinates so /Rect == BBox.
void regenAppearance(QPDF& qpdf, QPDFObjectHandle annot, const QString& text) {
    double rc[4];
    if (!box4(annot.getKey("/Rect"), rc))
        return;
    const double x0 = std::min(rc[0], rc[2]), y0 = std::min(rc[1], rc[3]);
    const double x1 = std::max(rc[0], rc[2]), y1 = std::max(rc[1], rc[3]);

    double r = 0, g = 0, b = 0;
    double cc[3];
    QPDFObjectHandle c = annot.getKey("/C");
    if (c.isArray() && c.getArrayNItems() == 3) {
        for (int i = 0; i < 3; ++i)
            cc[i] = c.getArrayItem(i).getNumericValue();
        r = cc[0];
        g = cc[1];
        b = cc[2];
    }

    const double size = std::max(7.0, std::min(12.0, (y1 - y0) - 4));
    QPDFObjectHandle helv = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
    helv.replaceKey("/Type", QPDFObjectHandle::newName("/Font"));
    helv.replaceKey("/Subtype", QPDFObjectHandle::newName("/Type1"));
    helv.replaceKey("/BaseFont", QPDFObjectHandle::newName("/Helvetica"));
    QPDFObjectHandle fonts = QPDFObjectHandle::newDictionary();
    fonts.replaceKey("/Helv", helv);
    QPDFObjectHandle res = QPDFObjectHandle::newDictionary();
    res.replaceKey("/Font", fonts);

    const std::string da =
        "/Helv " + num(size) + " Tf " + num(r) + " " + num(g) + " " + num(b) + " rg";
    const std::string draw =
        num(r) + " " + num(g) + " " + num(b) + " RG 1.5 w " + num(x0 + 0.75) + " " +
        num(y0 + 0.75) + " " + num(x1 - x0 - 1.5) + " " + num(y1 - y0 - 1.5) + " re S\n" + "BT " +
        da + " " + num(x0 + 3) + " " + num(y1 - size - 2) + " Td " + litStr(text) + " Tj ET";

    QPDFObjectHandle form = QPDFObjectHandle::newStream(&qpdf, draw);
    QPDFObjectHandle fd = form.getDict();
    fd.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    fd.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
    fd.replaceKey("/Resources", res);
    QPDFObjectHandle bbox = QPDFObjectHandle::newArray();
    for (double v : {x0, y0, x1, y1})
        bbox.appendItem(QPDFObjectHandle::newReal(num(v)));
    fd.replaceKey("/BBox", bbox);

    QPDFObjectHandle ap = QPDFObjectHandle::newDictionary();
    ap.replaceKey("/N", form);
    annot.replaceKey("/AP", ap);
    annot.replaceKey("/DA", QPDFObjectHandle::newString(da));
}

// Run `mutate` on the first /FreeText annotation matching obj/gen, then write.
// `mutate(page, annots, index)` performs the edit. Returns false if not found.
bool editMatching(const QString& inputPath, const QString& outputPath, int objId, int objGen,
                  QString* error,
                  const std::function<void(QPDFObjectHandle&, QPDFObjectHandle&, int)>& mutate) {
    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;
    const QPDFObjGen want(objId, objGen);

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());
        QPDFPageDocumentHelper dh(qpdf);
        bool found = false;
        for (QPDFPageObjectHelper& page : dh.getAllPages()) {
            QPDFObjectHandle pageObj = page.getObjectHandle();
            QPDFObjectHandle annots = pageObj.getKey("/Annots");
            if (!annots.isArray())
                continue;
            for (int i = 0; i < annots.getArrayNItems(); ++i) {
                if (annots.getArrayItem(i).getObjGen() == want) {
                    mutate(pageObj, annots, i);
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }
        if (!found) {
            if (error)
                *error = QStringLiteral("That text box is no longer in the document.");
            if (inPlace)
                QFile::remove(target);
            return false;
        }
        QPDFWriter writer(qpdf, target.toLocal8Bit().constData());
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
                *error = QStringLiteral("The edited file couldn't replace the original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}
} // namespace

QList<TextEditor::TextBox> TextEditor::read(const QString& path) {
    QList<TextBox> out;
    try {
        QPDF qpdf;
        qpdf.processFile(path.toLocal8Bit().constData());
        QPDFPageDocumentHelper dh(qpdf);
        dh.pushInheritedAttributesToPage();
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        for (int p = 0; p < static_cast<int>(pages.size()); ++p) {
            QPDFObjectHandle pageObj = pages[p].getObjectHandle();
            QPDFObjectHandle annots = pageObj.getKey("/Annots");
            if (!annots.isArray())
                continue;
            double mb[4];
            if (!box4(pageObj.getKey("/MediaBox"), mb))
                continue;
            const double llx = std::min(mb[0], mb[2]), lly = std::min(mb[1], mb[3]);
            const double w = std::abs(mb[2] - mb[0]), h = std::abs(mb[3] - mb[1]);
            if (w <= 0 || h <= 0)
                continue;
            for (int i = 0; i < annots.getArrayNItems(); ++i) {
                QPDFObjectHandle an = annots.getArrayItem(i);
                QPDFObjectHandle st = an.getKey("/Subtype");
                if (!st.isName() || st.getName() != "/FreeText")
                    continue;
                double rc[4];
                if (!box4(an.getKey("/Rect"), rc))
                    continue;
                const double x0 = std::min(rc[0], rc[2]), x1 = std::max(rc[0], rc[2]);
                const double y0 = std::min(rc[1], rc[3]), y1 = std::max(rc[1], rc[3]);
                TextBox tb;
                tb.page = p;
                tb.rect = QRectF((x0 - llx) / w, (lly + h - y1) / h, (x1 - x0) / w, (y1 - y0) / h);
                QPDFObjectHandle contents = an.getKey("/Contents");
                tb.text = contents.isString() ? QString::fromStdString(contents.getUTF8Value())
                                              : QString();
                tb.objId = an.getObjGen().getObj();
                tb.objGen = an.getObjGen().getGen();
                out.append(tb);
            }
        }
    } catch (const std::exception&) {
        return {};
    }
    return out;
}

bool TextEditor::setText(const QString& inputPath, const QString& outputPath, int objId, int objGen,
                         const QString& text, QString* error) {
    return editMatching(inputPath, outputPath, objId, objGen, error,
                        [&](QPDFObjectHandle&, QPDFObjectHandle& annots, int i) {
                            QPDFObjectHandle an = annots.getArrayItem(i);
                            an.replaceKey("/Contents",
                                          QPDFObjectHandle::newUnicodeString(text.toStdString()));
                            if (QPDF* owner = an.getOwningQPDF())
                                regenAppearance(*owner, an, text);
                        });
}

bool TextEditor::remove(const QString& inputPath, const QString& outputPath, int objId, int objGen,
                        QString* error) {
    return editMatching(inputPath, outputPath, objId, objGen, error,
                        [](QPDFObjectHandle&, QPDFObjectHandle& annots, int i) {
                            annots.eraseItem(i);
                        });
}
