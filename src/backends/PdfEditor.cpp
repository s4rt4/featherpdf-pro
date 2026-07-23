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

#include "backends/PdfEditor.h"

#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>

#include <algorithm>
#include <exception>
#include <functional>
#include <memory>
#include <vector>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

bool PdfEditor::saveArrangement(const QString& inputPath, const QString& outputPath,
                                const QVector<int>& order, const QVector<int>& rotations,
                                QString* error) {
    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    // Never write over the file we are reading - go through a sibling temp file.
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        QPDFPageDocumentHelper dh(qpdf);
        // Make each page self-contained (own MediaBox/Resources) before we detach
        // them from the page tree, so re-adding in a new order keeps fidelity.
        dh.pushInheritedAttributesToPage();
        std::vector<QPDFPageObjectHelper> original = dh.getAllPages();

        // Detach every page, then re-add only those in `order`, in that order,
        // each rotated as requested. Dropped pages simply never come back.
        for (auto& page : original)
            dh.removePage(page);

        const int n = static_cast<int>(original.size());
        for (int slot = 0; slot < order.size(); ++slot) {
            const int orig = order[slot];
            if (orig < 0 || orig >= n)
                continue;
            QPDFPageObjectHelper page = original[orig];
            int rot = (slot < rotations.size()) ? rotations[slot] : 0;
            rot = ((rot % 360) + 360) % 360;
            if (rot != 0)
                page.rotatePage(rot, /*relative=*/true);
            dh.addPage(page, /*first=*/false);
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
        QFile::remove(outputPath); // rename won't clobber an existing file
        if (!QFile::rename(target, outputPath)) {
            if (error)
                *error = QStringLiteral("The edited file couldn't replace the original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}

bool PdfEditor::setOutline(const QString& inputPath, const QString& outputPath,
                           const QVector<OutlineItem>& items, QString* error) {
    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        QPDFPageDocumentHelper dh(qpdf);
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        const int n = static_cast<int>(pages.size());
        QPDFObjectHandle root = qpdf.getRoot();

        if (items.isEmpty()) {
            root.removeKey("/Outlines");
            QPDFObjectHandle pm = root.getKey("/PageMode");
            if (pm.isName() && pm.getName() == "/UseOutlines")
                root.removeKey("/PageMode");
        } else {
            QPDFObjectHandle outlines = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
            outlines.replaceKey("/Type", QPDFObjectHandle::newName("/Outlines"));

            // Build a sibling chain under `parent`, recursing into children. Returns
            // the number of items in the whole subtree (every level is left open, so
            // that count is what each /Count should report). firstOut/lastOut receive
            // the chain's end nodes for the parent's /First and /Last.
            std::function<int(const QVector<OutlineItem>&, QPDFObjectHandle, QPDFObjectHandle&,
                              QPDFObjectHandle&)>
                build = [&](const QVector<OutlineItem>& list, QPDFObjectHandle parent,
                            QPDFObjectHandle& firstOut, QPDFObjectHandle& lastOut) -> int {
                bool havePrev = false;
                QPDFObjectHandle prev, firstItem, lastItem;
                int visible = 0;
                for (const OutlineItem& it : list) {
                    QPDFObjectHandle node =
                        qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
                    node.replaceKey("/Title",
                                    QPDFObjectHandle::newUnicodeString(it.title.toStdString()));
                    node.replaceKey("/Parent", parent);
                    if (n > 0) {
                        const int pg = std::max(0, std::min(it.page, n - 1));
                        QPDFObjectHandle dest = QPDFObjectHandle::newArray();
                        dest.appendItem(pages[pg].getObjectHandle());
                        dest.appendItem(QPDFObjectHandle::newName("/Fit"));
                        node.replaceKey("/Dest", dest);
                    }

                    QPDFObjectHandle cFirst, cLast;
                    const int childCount = build(it.children, node, cFirst, cLast);
                    if (childCount > 0) {
                        node.replaceKey("/First", cFirst);
                        node.replaceKey("/Last", cLast);
                        node.replaceKey("/Count", QPDFObjectHandle::newInteger(childCount));
                    }

                    if (havePrev) {
                        prev.replaceKey("/Next", node);
                        node.replaceKey("/Prev", prev);
                    } else {
                        firstItem = node;
                    }
                    lastItem = node;
                    prev = node;
                    havePrev = true;
                    visible += 1 + childCount;
                }
                firstOut = firstItem;
                lastOut = lastItem;
                return visible;
            };

            QPDFObjectHandle first, last;
            const int total = build(items, outlines, first, last);
            if (total > 0) {
                outlines.replaceKey("/First", first);
                outlines.replaceKey("/Last", last);
                outlines.replaceKey("/Count", QPDFObjectHandle::newInteger(total));
            }
            root.replaceKey("/Outlines", outlines);
            root.replaceKey("/PageMode", QPDFObjectHandle::newName("/UseOutlines"));
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
                *error = QStringLiteral("The file with the new outline couldn't replace the "
                                        "original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}

bool PdfEditor::insertPages(const QString& inputPath, const QString& outputPath,
                            const QVector<int>& order, const QVector<int>& rotations,
                            const QString& insertPath, const QVector<int>& insertPages, int atSlot,
                            QString* error) {
    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    const int clampedAt = std::max(0, std::min<int>(atSlot, order.size()));

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        QPDFPageDocumentHelper dh(qpdf);
        dh.pushInheritedAttributesToPage();
        std::vector<QPDFPageObjectHelper> base = dh.getAllPages();
        for (auto& page : base)
            dh.removePage(page);

        // The insert source must outlive write(): QPDF copies foreign pages
        // lazily, so keep it open until the file is flushed. addPage() copies a
        // foreign page automatically when it belongs to a different QPDF.
        QPDF source;
        source.processFile(insertPath.toLocal8Bit().constData());
        QPDFPageDocumentHelper srcDh(source);
        srcDh.pushInheritedAttributesToPage();
        std::vector<QPDFPageObjectHelper> srcPages = srcDh.getAllPages();
        const int srcN = static_cast<int>(srcPages.size());

        const auto addInserted = [&] {
            for (int p : insertPages)
                if (p >= 0 && p < srcN)
                    dh.addPage(srcPages[p], /*first=*/false);
        };

        const int n = static_cast<int>(base.size());
        for (int slot = 0; slot < order.size(); ++slot) {
            if (slot == clampedAt)
                addInserted();
            const int orig = order[slot];
            if (orig < 0 || orig >= n)
                continue;
            QPDFPageObjectHelper page = base[orig];
            int rot = (slot < rotations.size()) ? rotations[slot] : 0;
            rot = ((rot % 360) + 360) % 360;
            if (rot != 0)
                page.rotatePage(rot, /*relative=*/true);
            dh.addPage(page, /*first=*/false);
        }
        if (clampedAt >= order.size()) // inserting at (or past) the end
            addInserted();

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
                *error = QStringLiteral("The merged file couldn't replace the original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}

namespace {
// A page box as [x0,y0,x1,y1] with x0<x1, y0<y1 (PDF points, y up).
struct Box {
    double x0, y0, x1, y1;
    bool valid = false;
};
Box readBox(QPDFObjectHandle page) {
    QPDFObjectHandle b = page.getKey("/CropBox");
    if (!b.isArray() || b.getArrayNItems() != 4)
        b = page.getKey("/MediaBox");
    Box out;
    if (!b.isArray() || b.getArrayNItems() != 4)
        return out;
    double v[4];
    for (int i = 0; i < 4; ++i) {
        QPDFObjectHandle e = b.getArrayItem(i);
        if (!e.isNumber())
            return out;
        v[i] = e.getNumericValue();
    }
    out.x0 = std::min(v[0], v[2]);
    out.y0 = std::min(v[1], v[3]);
    out.x1 = std::max(v[0], v[2]);
    out.y1 = std::max(v[1], v[3]);
    out.valid = true;
    return out;
}
} // namespace

bool PdfEditor::cropPages(const QString& inputPath, const QString& outputPath,
                          const QVector<int>& order, const QVector<int>& rotations,
                          const QVector<int>& applyToSlots, const CropMargins& margins,
                          QString* error) {
    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    const bool cropAll = applyToSlots.isEmpty();
    const auto cropsThisSlot = [&](int slot) {
        return cropAll || applyToSlots.contains(slot);
    };

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        QPDFPageDocumentHelper dh(qpdf);
        dh.pushInheritedAttributesToPage();
        std::vector<QPDFPageObjectHelper> base = dh.getAllPages();
        for (auto& page : base)
            dh.removePage(page);

        const int n = static_cast<int>(base.size());
        for (int slot = 0; slot < order.size(); ++slot) {
            const int orig = order[slot];
            if (orig < 0 || orig >= n)
                continue;
            QPDFPageObjectHelper page = base[orig];
            QPDFObjectHandle obj = page.getObjectHandle();

            int sessionRot = (slot < rotations.size()) ? rotations[slot] : 0;
            sessionRot = ((sessionRot % 360) + 360) % 360;

            if (cropsThisSlot(slot)) {
                // The page's displayed orientation includes any baked-in /Rotate
                // plus the per-slot session rotation. Map the displayed margins
                // into the page's unrotated coordinates before insetting the box.
                QPDFObjectHandle rotObj = obj.getKey("/Rotate");
                int baked = rotObj.isInteger() ? rotObj.getIntValueAsInt() : 0;
                const int eff = (((baked + sessionRot) % 360) + 360) % 360;

                const double L = margins.left, R = margins.right, T = margins.top,
                             B = margins.bottom;
                double uL = L, uR = R, uT = T, uB = B; // unrotated insets
                switch (eff) {
                case 90:  uL = T; uR = B; uT = R; uB = L; break;
                case 180: uL = R; uR = L; uT = B; uB = T; break;
                case 270: uL = B; uR = T; uT = L; uB = R; break;
                default:  break; // 0
                }

                const Box box = readBox(obj);
                if (box.valid) {
                    const double nx0 = box.x0 + uL, nx1 = box.x1 - uR;
                    const double ny0 = box.y0 + uB, ny1 = box.y1 - uT;
                    // Only apply when a sane, non-degenerate box remains.
                    if (nx1 - nx0 >= 1.0 && ny1 - ny0 >= 1.0) {
                        QPDFObjectHandle cb = QPDFObjectHandle::newArray();
                        cb.appendItem(QPDFObjectHandle::newReal(nx0, 3));
                        cb.appendItem(QPDFObjectHandle::newReal(ny0, 3));
                        cb.appendItem(QPDFObjectHandle::newReal(nx1, 3));
                        cb.appendItem(QPDFObjectHandle::newReal(ny1, 3));
                        obj.replaceKey("/CropBox", cb);
                    }
                }
            }

            if (sessionRot != 0)
                page.rotatePage(sessionRot, /*relative=*/true);
            dh.addPage(page, /*first=*/false);
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
                *error = QStringLiteral("The cropped file couldn't replace the original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}

bool PdfEditor::combine(const QStringList& inputPaths, const QString& outputPath, QString* error) {
    if (inputPaths.isEmpty()) {
        if (error)
            *error = QStringLiteral("No files to combine.");
        return false;
    }

    // Always write to a sibling temp file, then rename over the target - this
    // keeps every input intact during the (lazy) read/write even when the output
    // path equals one of the inputs.
    const QString target = outputPath + QStringLiteral(".feather-tmp");

    try {
        QPDF out;
        out.emptyPDF();
        QPDFPageDocumentHelper outDH(out);

        // The source documents must outlive write(): QPDF copies foreign page
        // objects lazily, so keep every input open until the file is flushed.
        std::vector<std::unique_ptr<QPDF>> sources;
        sources.reserve(inputPaths.size());

        for (const QString& path : inputPaths) {
            auto in = std::make_unique<QPDF>();
            in->processFile(path.toLocal8Bit().constData());
            QPDFPageDocumentHelper inDH(*in);
            inDH.pushInheritedAttributesToPage(); // self-contained pages survive the copy
            for (QPDFPageObjectHelper& page : inDH.getAllPages())
                outDH.addPage(page, /*first=*/false);
            sources.push_back(std::move(in));
        }

        QPDFWriter writer(out, target.toLocal8Bit().constData());
        writer.write();
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        QFile::remove(target);
        return false;
    }

    QFile::remove(outputPath); // rename won't clobber an existing file
    if (!QFile::rename(target, outputPath)) {
        if (error)
            *error = QStringLiteral("The combined file couldn't be written.");
        QFile::remove(target);
        return false;
    }
    return true;
}

bool PdfEditor::protect(const QString& inputPath, const QString& outputPath,
                        const QString& openPassword, const Permissions& perms, QString* error) {
    const bool restricted =
        !(perms.allowPrinting && perms.allowCopying && perms.allowEditing);
    if (openPassword.isEmpty() && !restricted) {
        if (error)
            *error = QStringLiteral("Set a password or restrict at least one permission.");
        return false;
    }

    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    // The owner password enforces the restrictions. When nothing is restricted
    // we mirror the open password (simple "password to open"); when something is
    // restricted we use a random owner password so the limits actually bite even
    // for someone who knows the open password. (Removing protection later only
    // needs the open password, which is what the app remembers.)
    QByteArray owner;
    if (restricted) {
        for (int i = 0; i < 32; ++i)
            owner.append(char('!' + (QRandomGenerator::global()->bounded(93))));
    } else {
        owner = openPassword.toUtf8();
    }

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        QPDFWriter writer(qpdf, target.toLocal8Bit().constData());
        // R6 = AES-256, the only scheme PDF 2.0 defines. Accessibility extraction
        // stays allowed regardless, per accessibility guidance.
        const QByteArray user = openPassword.toUtf8();
        writer.setR6EncryptionParameters(
            user.constData(), owner.constData(),
            /*allow_accessibility=*/true, /*allow_extract=*/perms.allowCopying,
            /*allow_assemble=*/perms.allowEditing, /*allow_annotate_and_form=*/perms.allowEditing,
            /*allow_form_filling=*/perms.allowEditing, /*allow_modify_other=*/perms.allowEditing,
            perms.allowPrinting ? qpdf_r3p_full : qpdf_r3p_none, /*encrypt_metadata_aes=*/true);
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
                *error = QStringLiteral("The protected file couldn't replace the original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}

namespace {
// Build a new image-only page: an /Image XObject (the flattened JPEG) drawn to
// fill a MediaBox of the page's point size. Returned as an indirect page object.
QPDFObjectHandle makeImagePage(QPDF& pdf, const PdfEditor::RedactedImage& ri) {
    QPDFObjectHandle image = QPDFObjectHandle::newStream(&pdf);
    QPDFObjectHandle d = image.getDict();
    d.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    d.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
    d.replaceKey("/Width", QPDFObjectHandle::newInteger(ri.pxWidth));
    d.replaceKey("/Height", QPDFObjectHandle::newInteger(ri.pxHeight));
    d.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceRGB"));
    d.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
    image.replaceStreamData(std::string(ri.jpeg.constData(), ri.jpeg.size()),
                            QPDFObjectHandle::newName("/DCTDecode"), QPDFObjectHandle::newNull());

    QPDFObjectHandle xobjs = QPDFObjectHandle::newDictionary();
    xobjs.replaceKey("/Im0", image);
    QPDFObjectHandle resources = QPDFObjectHandle::newDictionary();
    resources.replaceKey("/XObject", xobjs);

    const auto num = [](double v) { return QByteArray::number(v, 'f', 3).toStdString(); };
    const std::string content =
        "q\n" + num(ri.ptWidth) + " 0 0 " + num(ri.ptHeight) + " 0 0 cm\n/Im0 Do\nQ\n";
    QPDFObjectHandle contents = QPDFObjectHandle::newStream(&pdf, content);

    QPDFObjectHandle mediabox = QPDFObjectHandle::newArray();
    mediabox.appendItem(QPDFObjectHandle::newInteger(0));
    mediabox.appendItem(QPDFObjectHandle::newInteger(0));
    mediabox.appendItem(QPDFObjectHandle::newReal(num(ri.ptWidth)));
    mediabox.appendItem(QPDFObjectHandle::newReal(num(ri.ptHeight)));

    QPDFObjectHandle page = QPDFObjectHandle::newDictionary();
    page.replaceKey("/Type", QPDFObjectHandle::newName("/Page"));
    page.replaceKey("/MediaBox", mediabox);
    page.replaceKey("/Resources", resources);
    page.replaceKey("/Contents", contents);
    return pdf.makeIndirectObject(page);
}
} // namespace

bool PdfEditor::saveRedacted(const QString& inputPath, const QString& outputPath,
                             const QVector<int>& order, const QVector<int>& rotations,
                             const QHash<int, RedactedImage>& redactedBySlot, QString* error) {
    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        QPDFPageDocumentHelper dh(qpdf);
        dh.pushInheritedAttributesToPage();
        std::vector<QPDFPageObjectHelper> original = dh.getAllPages();
        for (auto& page : original)
            dh.removePage(page);

        const int n = static_cast<int>(original.size());
        for (int slot = 0; slot < order.size(); ++slot) {
            if (auto it = redactedBySlot.constFind(slot); it != redactedBySlot.constEnd()) {
                // Replace this slot with a flattened image-only page (rotation is
                // already baked into the render, so the new page needs no /Rotate).
                QPDFPageObjectHelper ph(makeImagePage(qpdf, it.value()));
                dh.addPage(ph, /*first=*/false);
                continue;
            }
            const int orig = order[slot];
            if (orig < 0 || orig >= n)
                continue;
            QPDFPageObjectHelper page = original[orig];
            int rot = (slot < rotations.size()) ? rotations[slot] : 0;
            rot = ((rot % 360) + 360) % 360;
            if (rot != 0)
                page.rotatePage(rot, /*relative=*/true);
            dh.addPage(page, /*first=*/false);
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
                *error = QStringLiteral("The redacted file couldn't replace the original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}

bool PdfEditor::removeProtection(const QString& inputPath, const QString& outputPath,
                                 const QString& password, QString* error) {
    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData(), password.toUtf8().constData());

        QPDFWriter writer(qpdf, target.toLocal8Bit().constData());
        writer.setPreserveEncryption(false); // drop the encryption layer
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
                *error = QStringLiteral("The unprotected file couldn't replace the original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}

bool PdfEditor::optimize(const QString& inputPath, const QString& outputPath, qint64* beforeBytes,
                         qint64* afterBytes, QString* error) {
    if (beforeBytes)
        *beforeBytes = QFileInfo(inputPath).size();

    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());
        QPDFWriter writer(qpdf, target.toLocal8Bit().constData());
        writer.setObjectStreamMode(qpdf_o_generate); // pack indirect objects together
        writer.setCompressStreams(true);             // compress uncompressed streams
        writer.setRecompressFlate(true);             // recompress flate at the best level
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
                *error = QStringLiteral("The optimized file couldn't replace the original.");
            QFile::remove(target);
            return false;
        }
    }
    if (afterBytes)
        *afterBytes = QFileInfo(outputPath).size();
    return true;
}

bool PdfEditor::isPasswordProtected(const QString& path) {
    try {
        QPDF qpdf;
        qpdf.processFile(path.toLocal8Bit().constData());
        // Opened with no password: it's only "needs a password" if it carries an
        // encryption layer (an empty user password but owner restrictions).
        return qpdf.isEncrypted();
    } catch (const QPDFExc& e) {
        // A wrong/missing password throws specifically with qpdf_e_password.
        return e.getErrorCode() == qpdf_e_password;
    } catch (const std::exception&) {
        return false;
    }
}
