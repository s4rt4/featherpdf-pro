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

#include "backends/Sanitizer.h"

#include <QFile>
#include <QFileInfo>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

namespace {
// Number of name→value entries in a PDF name tree's /Names array (each entry is
// a key string followed by its value).
int nameTreeCount(QPDFObjectHandle node) {
    if (!node.isDictionary())
        return 0;
    QPDFObjectHandle names = node.getKey("/Names");
    return names.isArray() ? names.getArrayNItems() / 2 : 0;
}
}

bool Sanitizer::sanitize(const QString& inputPath, const QString& outputPath,
                         const Options& options, Report* report, QString* error) {
    Report rep;
    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());
        QPDFObjectHandle root = qpdf.getRoot();
        QPDFObjectHandle names = root.getKey("/Names"); // catalog name trees

        if (options.metadata) {
            QPDFObjectHandle trailer = qpdf.getTrailer();
            if (trailer.hasKey("/Info")) {
                trailer.removeKey("/Info");
                ++rep.metadata;
            }
            if (root.hasKey("/Metadata")) { // XMP packet
                root.removeKey("/Metadata");
                ++rep.metadata;
            }
        }

        if (options.attachments) {
            if (names.isDictionary() && names.hasKey("/EmbeddedFiles")) {
                rep.attachments += nameTreeCount(names.getKey("/EmbeddedFiles"));
                names.removeKey("/EmbeddedFiles");
            }
            if (root.hasKey("/AF")) { // associated files
                root.removeKey("/AF");
            }
            // Drop FileAttachment annotations from every page.
            QPDFPageDocumentHelper dh(qpdf);
            for (QPDFPageObjectHelper& page : dh.getAllPages()) {
                QPDFObjectHandle pageObj = page.getObjectHandle();
                QPDFObjectHandle annots = pageObj.getKey("/Annots");
                if (!annots.isArray())
                    continue;
                QPDFObjectHandle kept = QPDFObjectHandle::newArray();
                for (int i = 0; i < annots.getArrayNItems(); ++i) {
                    QPDFObjectHandle a = annots.getArrayItem(i);
                    QPDFObjectHandle sub = a.getKey("/Subtype");
                    if (sub.isName() && sub.getName() == "/FileAttachment") {
                        ++rep.attachments;
                        continue;
                    }
                    kept.appendItem(a);
                }
                if (kept.getArrayNItems() == 0)
                    pageObj.removeKey("/Annots");
                else
                    pageObj.replaceKey("/Annots", kept);
            }
        }

        if (options.scripts) {
            if (names.isDictionary() && names.hasKey("/JavaScript")) {
                rep.scripts += nameTreeCount(names.getKey("/JavaScript"));
                names.removeKey("/JavaScript");
            }
            // Document-level open/additional actions.
            QPDFObjectHandle openAction = root.getKey("/OpenAction");
            if (openAction.isDictionary()) {
                QPDFObjectHandle s = openAction.getKey("/S");
                if (s.isName() && s.getName() == "/JavaScript") {
                    root.removeKey("/OpenAction");
                    ++rep.scripts;
                }
            }
            if (root.hasKey("/AA")) {
                root.removeKey("/AA");
                ++rep.scripts;
            }
            // Page-level additional actions.
            QPDFPageDocumentHelper dh(qpdf);
            for (QPDFPageObjectHelper& page : dh.getAllPages()) {
                QPDFObjectHandle pageObj = page.getObjectHandle();
                if (pageObj.hasKey("/AA")) {
                    pageObj.removeKey("/AA");
                    ++rep.scripts;
                }
            }
        }

        // If the catalog name tree is now empty, drop it too.
        if (names.isDictionary() && names.getKeys().empty())
            root.removeKey("/Names");

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
                *error = QStringLiteral("The cleaned file couldn't replace the original.");
            QFile::remove(target);
            return false;
        }
    }

    if (report)
        *report = rep;
    return true;
}
