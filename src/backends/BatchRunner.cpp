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

#include "backends/BatchRunner.h"

#include "backends/Flattener.h"
#include "backends/Ocr.h"
#include "backends/Optimizer.h"
#include "backends/PdfA.h"
#include "backends/PdfEditor.h"
#include "backends/Sanitizer.h"
#include "backends/Watermarker.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSaveFile>
#include <QTemporaryDir>

#include <memory>

#include <poppler-qt6.h>

namespace {

int pageCount(const QString& path) {
    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(path);
    return (doc && !doc->isLocked()) ? doc->numPages() : 0;
}

bool applyStep(const BatchRunner::Step& s, const QString& in, const QString& out, QString* error) {
    const QVariantMap& p = s.params;
    switch (s.op) {
    case BatchRunner::Op::Optimize: {
        Optimizer::Options o;
        o.targetDpi = p.value(QStringLiteral("dpi"), 150).toInt();
        o.dropFonts = p.value(QStringLiteral("unembedFonts"), false).toBool();
        o.compress = p.value(QStringLiteral("compress"), true).toBool();
        Optimizer::Report r;
        return Optimizer::optimize(in, out, o, &r, error);
    }
    case BatchRunner::Op::Watermark: {
        Watermarker::WatermarkOptions o;
        o.text = p.value(QStringLiteral("text")).toString();
        o.opacity = p.value(QStringLiteral("opacity"), 0.30).toDouble();
        o.fontSize = p.value(QStringLiteral("fontSize"), 64.0).toDouble();
        o.rotationDeg = p.value(QStringLiteral("angle"), 45.0).toDouble();
        return Watermarker::addWatermark(in, out, o, error);
    }
    case BatchRunner::Op::Bates: {
        Watermarker::BatesOptions o;
        o.prefix = p.value(QStringLiteral("prefix")).toString();
        o.suffix = p.value(QStringLiteral("suffix")).toString();
        o.start = p.value(QStringLiteral("start"), 1).toInt();
        o.digits = p.value(QStringLiteral("digits"), 6).toInt();
        return Watermarker::addBates(in, out, o, error);
    }
    case BatchRunner::Op::HeaderFooter: {
        Watermarker::HeaderFooterOptions o;
        o.headerLeft = p.value(QStringLiteral("headerLeft")).toString();
        o.headerCenter = p.value(QStringLiteral("headerCenter")).toString();
        o.headerRight = p.value(QStringLiteral("headerRight")).toString();
        o.footerLeft = p.value(QStringLiteral("footerLeft")).toString();
        o.footerCenter = p.value(QStringLiteral("footerCenter")).toString();
        o.footerRight = p.value(QStringLiteral("footerRight")).toString();
        o.startNumber = p.value(QStringLiteral("startNumber"), 1).toInt();
        return Watermarker::addHeaderFooter(in, out, o, error);
    }
    case BatchRunner::Op::Sanitize: {
        Sanitizer::Options o;
        o.metadata = p.value(QStringLiteral("metadata"), true).toBool();
        o.attachments = p.value(QStringLiteral("attachments"), true).toBool();
        o.scripts = p.value(QStringLiteral("scripts"), true).toBool();
        Sanitizer::Report r;
        return Sanitizer::sanitize(in, out, o, &r, error);
    }
    case BatchRunner::Op::Encrypt: {
        PdfEditor::Permissions perms;
        perms.allowPrinting = p.value(QStringLiteral("allowPrint"), true).toBool();
        perms.allowCopying = p.value(QStringLiteral("allowCopy"), true).toBool();
        perms.allowEditing = p.value(QStringLiteral("allowEdit"), true).toBool();
        return PdfEditor::protect(in, out, p.value(QStringLiteral("password")).toString(), perms,
                                  error);
    }
    case BatchRunner::Op::Rotate: {
        const int angle = p.value(QStringLiteral("angle"), 90).toInt();
        const int n = pageCount(in);
        if (n <= 0) {
            if (error)
                *error = QObject::tr("couldn't read the page count");
            return false;
        }
        QVector<int> order, rotations;
        for (int i = 0; i < n; ++i) {
            order.append(i);
            rotations.append(angle);
        }
        return PdfEditor::saveArrangement(in, out, order, rotations, error);
    }
    case BatchRunner::Op::Flatten: {
        if (p.value(QStringLiteral("raster"), false).toBool())
            return Flattener::flattenRaster(in, out, p.value(QStringLiteral("dpi"), 150).toInt(),
                                            error);
        return Flattener::flattenLossless(in, out, error);
    }
    case BatchRunner::Op::PdfA:
        return PdfA::convertToPdfA1b(in, out, error);
    case BatchRunner::Op::Ocr: {
        if (!Ocr::isAvailable()) {
            if (error)
                *error = QObject::tr("Tesseract isn't installed");
            return false;
        }
        Ocr::Options o;
        o.deskew = p.value(QStringLiteral("deskew"), true).toBool();
        o.despeckle = p.value(QStringLiteral("despeckle"), true).toBool();
        o.binarize = p.value(QStringLiteral("binarize"), true).toBool();
        o.autoLanguage = p.value(QStringLiteral("autoLanguage"), false).toBool();
        return Ocr::addTextLayer(in, out, p.value(QStringLiteral("language"),
                                                  QStringLiteral("eng")).toString(),
                                 o, error);
    }
    }
    return false;
}

} // namespace

bool BatchRunner::runFile(const QString& inputPath, const QString& outputPath,
                          const QList<Step>& steps, QString* error) {
    if (steps.isEmpty()) {
        QFile::remove(outputPath);
        if (!QFile::copy(inputPath, outputPath)) {
            if (error)
                *error = QObject::tr("couldn't write the output file");
            return false;
        }
        return true;
    }

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        if (error)
            *error = QObject::tr("couldn't create a temporary working directory");
        return false;
    }

    QString current = inputPath;
    for (int i = 0; i < steps.size(); ++i) {
        const bool last = i == steps.size() - 1;
        const QString next =
            last ? outputPath : tmp.filePath(QStringLiteral("step%1.pdf").arg(i));
        QString why;
        if (!applyStep(steps[i], current, next, &why)) {
            if (error)
                *error = QStringLiteral("%1: %2").arg(opLabel(steps[i].op),
                                                      why.isEmpty() ? QObject::tr("failed") : why);
            return false;
        }
        current = next;
    }
    return true;
}

bool BatchRunner::opFromId(const QString& id, Op* op) {
    for (Op candidate : allOps()) {
        if (opId(candidate) == id) {
            if (op)
                *op = candidate;
            return true;
        }
    }
    return false;
}

bool BatchRunner::saveAction(const QString& path, const QList<Step>& steps, QString* error) {
    QJsonArray arr;
    for (const Step& s : steps) {
        QJsonObject o;
        o[QStringLiteral("op")] = opId(s.op);
        o[QStringLiteral("params")] = QJsonObject::fromVariantMap(s.params);
        arr.append(o);
    }
    QJsonObject root;
    root[QStringLiteral("feather-action")] = 1;
    root[QStringLiteral("steps")] = arr;

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QObject::tr("Couldn't write %1.").arg(path);
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        if (error)
            *error = QObject::tr("Couldn't save %1.").arg(path);
        return false;
    }
    return true;
}

bool BatchRunner::loadAction(const QString& path, QList<Step>* steps, QString* error) {
    const auto fail = [&](const QString& m) {
        if (error)
            *error = m;
        return false;
    };
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return fail(QObject::tr("Couldn't open %1.").arg(path));
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
        return fail(QObject::tr("%1 isn't a valid action file.").arg(path));
    const QJsonArray arr = doc.object().value(QStringLiteral("steps")).toArray();

    QList<Step> loaded;
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        Op op;
        if (!opFromId(o.value(QStringLiteral("op")).toString(), &op))
            return fail(QObject::tr("%1 names an unknown operation.").arg(path));
        // Start from the defaults so a step is complete even if the file predates
        // a parameter, then overlay whatever the file specifies.
        QVariantMap params = defaultParams(op);
        const QVariantMap saved = o.value(QStringLiteral("params")).toObject().toVariantMap();
        for (auto it = saved.constBegin(); it != saved.constEnd(); ++it)
            params[it.key()] = it.value();
        loaded.append({op, params});
    }
    if (steps)
        *steps = loaded;
    return true;
}

QString BatchRunner::opId(Op op) {
    switch (op) {
    case Op::Optimize: return QStringLiteral("optimize");
    case Op::Watermark: return QStringLiteral("watermark");
    case Op::Bates: return QStringLiteral("bates");
    case Op::HeaderFooter: return QStringLiteral("headerfooter");
    case Op::Sanitize: return QStringLiteral("sanitize");
    case Op::Encrypt: return QStringLiteral("encrypt");
    case Op::Rotate: return QStringLiteral("rotate");
    case Op::Flatten: return QStringLiteral("flatten");
    case Op::PdfA: return QStringLiteral("pdfa");
    case Op::Ocr: return QStringLiteral("ocr");
    }
    return QString();
}

QString BatchRunner::opLabel(Op op) {
    switch (op) {
    case Op::Optimize: return QObject::tr("Optimize");
    case Op::Watermark: return QObject::tr("Watermark");
    case Op::Bates: return QObject::tr("Bates numbering");
    case Op::HeaderFooter: return QObject::tr("Header & footer");
    case Op::Sanitize: return QObject::tr("Remove hidden info");
    case Op::Encrypt: return QObject::tr("Encrypt");
    case Op::Rotate: return QObject::tr("Rotate pages");
    case Op::Flatten: return QObject::tr("Flatten");
    case Op::PdfA: return QObject::tr("Convert to PDF/A");
    case Op::Ocr: return QObject::tr("Recognize text (OCR)");
    }
    return QString();
}

QVariantMap BatchRunner::defaultParams(Op op) {
    QVariantMap p;
    switch (op) {
    case Op::Optimize:
        p[QStringLiteral("dpi")] = 150;
        p[QStringLiteral("unembedFonts")] = false;
        p[QStringLiteral("compress")] = true;
        break;
    case Op::Watermark:
        p[QStringLiteral("text")] = QStringLiteral("CONFIDENTIAL");
        p[QStringLiteral("opacity")] = 0.30;
        p[QStringLiteral("fontSize")] = 64.0;
        p[QStringLiteral("angle")] = 45.0;
        break;
    case Op::Bates:
        p[QStringLiteral("prefix")] = QString();
        p[QStringLiteral("suffix")] = QString();
        p[QStringLiteral("start")] = 1;
        p[QStringLiteral("digits")] = 6;
        break;
    case Op::HeaderFooter:
        p[QStringLiteral("footerCenter")] = QStringLiteral("Page {n} of {p}");
        p[QStringLiteral("startNumber")] = 1;
        break;
    case Op::Sanitize:
        p[QStringLiteral("metadata")] = true;
        p[QStringLiteral("attachments")] = true;
        p[QStringLiteral("scripts")] = true;
        break;
    case Op::Encrypt:
        p[QStringLiteral("password")] = QString();
        p[QStringLiteral("allowPrint")] = true;
        p[QStringLiteral("allowCopy")] = true;
        p[QStringLiteral("allowEdit")] = true;
        break;
    case Op::Rotate:
        p[QStringLiteral("angle")] = 90;
        break;
    case Op::Flatten:
        p[QStringLiteral("raster")] = false;
        p[QStringLiteral("dpi")] = 150;
        break;
    case Op::PdfA:
        break;
    case Op::Ocr:
        p[QStringLiteral("language")] = QStringLiteral("eng");
        p[QStringLiteral("deskew")] = true;
        p[QStringLiteral("despeckle")] = true;
        p[QStringLiteral("binarize")] = true;
        p[QStringLiteral("autoLanguage")] = false;
        break;
    }
    return p;
}

QList<BatchRunner::Op> BatchRunner::allOps() {
    return {Op::Optimize, Op::Watermark, Op::Bates,   Op::HeaderFooter, Op::Sanitize,
            Op::Encrypt,  Op::Rotate,    Op::Flatten, Op::PdfA,         Op::Ocr};
}

QString BatchRunner::summarize(const Step& s) {
    const QVariantMap& p = s.params;
    const QString label = opLabel(s.op);
    switch (s.op) {
    case Op::Watermark:
        return QObject::tr("%1 “%2”").arg(label, p.value(QStringLiteral("text")).toString());
    case Op::Optimize:
        return QObject::tr("%1 (%2 DPI)").arg(label).arg(p.value(QStringLiteral("dpi")).toInt());
    case Op::Bates: {
        const QString pre = p.value(QStringLiteral("prefix")).toString();
        return pre.isEmpty() ? label : QObject::tr("%1 (%2…)").arg(label, pre);
    }
    case Op::Encrypt:
        return p.value(QStringLiteral("password")).toString().isEmpty()
            ? QObject::tr("%1 (permissions)").arg(label)
            : QObject::tr("%1 (password)").arg(label);
    case Op::Rotate:
        return QObject::tr("%1 %2°").arg(label).arg(p.value(QStringLiteral("angle")).toInt());
    case Op::Flatten:
        return p.value(QStringLiteral("raster")).toBool() ? QObject::tr("%1 (raster)").arg(label)
                                                          : label;
    case Op::Ocr:
        return QObject::tr("%1 [%2]").arg(label, p.value(QStringLiteral("language")).toString());
    default:
        return label;
    }
}
