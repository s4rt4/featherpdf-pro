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

#include "backends/LinkEditor.h"

#include <QFile>
#include <QFileInfo>

#include <algorithm>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

namespace {
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

// Normalized (top-left) rect -> PDF /Rect array on a page with MediaBox `mb`.
QPDFObjectHandle rectToPdf(const double mb[4], const QRectF& r) {
    const double W = mb[2] - mb[0], H = mb[3] - mb[1];
    const double x0 = mb[0] + r.left() * W, x1 = mb[0] + r.right() * W;
    const double yTop = mb[3] - r.top() * H, yBot = mb[3] - r.bottom() * H;
    QPDFObjectHandle a = QPDFObjectHandle::newArray();
    a.appendItem(QPDFObjectHandle::newReal(std::min(x0, x1), 2));
    a.appendItem(QPDFObjectHandle::newReal(std::min(yTop, yBot), 2));
    a.appendItem(QPDFObjectHandle::newReal(std::max(x0, x1), 2));
    a.appendItem(QPDFObjectHandle::newReal(std::max(yTop, yBot), 2));
    return a;
}

// PDF /Rect (bottom-left origin) -> normalized top-left fraction on MediaBox `mb`.
QRectF rectFromPdf(const double mb[4], QPDFObjectHandle rect) {
    if (!rect.isArray() || rect.getArrayNItems() != 4)
        return {};
    double v[4];
    for (int i = 0; i < 4; ++i)
        v[i] = rect.getArrayItem(i).getNumericValue();
    const double W = mb[2] - mb[0], H = mb[3] - mb[1];
    if (W <= 0 || H <= 0)
        return {};
    const double loX = std::min(v[0], v[2]), hiX = std::max(v[0], v[2]);
    const double loY = std::min(v[1], v[3]), hiY = std::max(v[1], v[3]);
    // y is measured from the bottom; flip to a top-left origin.
    return QRectF((loX - mb[0]) / W, (mb[3] - hiY) / H, (hiX - loX) / W, (hiY - loY) / H);
}

// A /Link annotation's URI, or empty if it isn't a URI action.
QString linkUri(QPDFObjectHandle annot, bool* isUri) {
    *isUri = false;
    QPDFObjectHandle a = annot.getKey("/A");
    if (!a.isDictionary())
        return {};
    QPDFObjectHandle s = a.getKey("/S");
    if (!s.isName() || s.getName() != "/URI")
        return {};
    QPDFObjectHandle uri = a.getKey("/URI");
    if (!uri.isString())
        return {};
    *isUri = true;
    return QString::fromStdString(uri.getStringValue());
}

QPDFObjectHandle uriAction(const QString& url) {
    QPDFObjectHandle a = QPDFObjectHandle::newDictionary();
    a.replaceKey("/Type", QPDFObjectHandle::newName("/Action"));
    a.replaceKey("/S", QPDFObjectHandle::newName("/URI"));
    a.replaceKey("/URI", QPDFObjectHandle::newString(url.toStdString()));
    return a;
}

bool inPlaceWrite(QPDF& qpdf, const QString& inputPath, const QString& outputPath, QString* error) {
    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;
    try {
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

QList<LinkEditor::Link> LinkEditor::read(const QString& path) {
    QList<Link> out;
    try {
        QPDF qpdf;
        qpdf.processFile(path.toLocal8Bit().constData());
        QPDFPageDocumentHelper dh(qpdf);
        int pageIndex = 0;
        for (QPDFPageObjectHelper& page : dh.getAllPages()) {
            QPDFObjectHandle pageObj = page.getObjectHandle();
            double mb[4];
            const bool haveMb = mediaBox(pageObj, mb);
            QPDFObjectHandle annots = pageObj.getKey("/Annots");
            if (annots.isArray()) {
                for (int i = 0; i < annots.getArrayNItems(); ++i) {
                    QPDFObjectHandle a = annots.getArrayItem(i);
                    QPDFObjectHandle sub = a.getKey("/Subtype");
                    if (!sub.isName() || sub.getName() != "/Link")
                        continue;
                    Link link;
                    link.objId = a.getObjGen().getObj();
                    link.objGen = a.getObjGen().getGen();
                    link.page = pageIndex;
                    if (haveMb)
                        link.rect = rectFromPdf(mb, a.getKey("/Rect"));
                    link.uri = linkUri(a, &link.isUri);
                    out.push_back(link);
                }
            }
            ++pageIndex;
        }
    } catch (const std::exception&) {
        // A read failure just yields no links.
    }
    return out;
}

bool LinkEditor::addUriLink(const QString& inputPath, const QString& outputPath, int page,
                            const QRectF& normRect, const QString& url, QString* error) {
    if (url.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("Enter a URL for the link.");
        return false;
    }
    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());
        QPDFPageDocumentHelper dh(qpdf);
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

        QPDFObjectHandle annot = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
        annot.replaceKey("/Type", QPDFObjectHandle::newName("/Annot"));
        annot.replaceKey("/Subtype", QPDFObjectHandle::newName("/Link"));
        annot.replaceKey("/Rect",
                         rectToPdf(mb, toUnrotatedFraction(normRect, pageRotate(pageObj))));
        QPDFObjectHandle border = QPDFObjectHandle::newArray();
        border.appendItem(QPDFObjectHandle::newInteger(0));
        border.appendItem(QPDFObjectHandle::newInteger(0));
        border.appendItem(QPDFObjectHandle::newInteger(0));
        annot.replaceKey("/Border", border); // invisible border
        annot.replaceKey("/A", uriAction(url.trimmed()));

        QPDFObjectHandle annots = pageObj.getKey("/Annots");
        if (!annots.isArray()) {
            annots = QPDFObjectHandle::newArray();
            pageObj.replaceKey("/Annots", annots);
        }
        annots.appendItem(annot);

        return inPlaceWrite(qpdf, inputPath, outputPath, error);
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }
}

bool LinkEditor::applyEdits(const QString& inputPath, const QString& outputPath,
                            const QList<Edit>& edits, QString* error) {
    if (edits.isEmpty()) {
        if (error)
            *error = QStringLiteral("No changes to apply.");
        return false;
    }
    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());
        QPDFPageDocumentHelper dh(qpdf);
        for (QPDFPageObjectHelper& page : dh.getAllPages()) {
            QPDFObjectHandle pageObj = page.getObjectHandle();
            QPDFObjectHandle annots = pageObj.getKey("/Annots");
            if (!annots.isArray())
                continue;
            QPDFObjectHandle kept = QPDFObjectHandle::newArray();
            for (int i = 0; i < annots.getArrayNItems(); ++i) {
                QPDFObjectHandle a = annots.getArrayItem(i);
                const int id = a.getObjGen().getObj(), gen = a.getObjGen().getGen();
                const Edit* match = nullptr;
                for (const Edit& e : edits)
                    if (e.objId == id && e.objGen == gen) {
                        match = &e;
                        break;
                    }
                if (match && match->remove)
                    continue; // drop it
                if (match && !match->newUri.trimmed().isEmpty())
                    a.replaceKey("/A", uriAction(match->newUri.trimmed()));
                kept.appendItem(a);
            }
            if (kept.getArrayNItems() == 0)
                pageObj.removeKey("/Annots");
            else
                pageObj.replaceKey("/Annots", kept);
        }
        return inPlaceWrite(qpdf, inputPath, outputPath, error);
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }
}
