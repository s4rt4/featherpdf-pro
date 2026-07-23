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

#include "backends/Optimizer.h"

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QImage>

#include <qpdf/Buffer.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

// Names listed in a stream's /Filter (which may be a single name or an array).
std::vector<std::string> filterNames(QPDFObjectHandle dict) {
    std::vector<std::string> out;
    QPDFObjectHandle f = dict.getKey("/Filter");
    if (f.isName()) {
        out.push_back(f.getName());
    } else if (f.isArray()) {
        for (int i = 0; i < f.getArrayNItems(); ++i) {
            QPDFObjectHandle item = f.getArrayItem(i);
            if (item.isName())
                out.push_back(item.getName());
        }
    }
    return out;
}

bool hasFilter(const std::vector<std::string>& filters, const char* name) {
    for (const std::string& f : filters)
        if (f == name)
            return true;
    return false;
}

// Raw (stored, still-compressed) byte size of a stream, or 0 if unreadable.
qint64 rawStreamSize(QPDFObjectHandle stream) {
    try {
        std::shared_ptr<Buffer> b = stream.getRawStreamData();
        return b ? static_cast<qint64>(b->getSize()) : 0;
    } catch (const std::exception&) {
        return 0;
    }
}

int intKey(QPDFObjectHandle dict, const char* key, int fallback) {
    QPDFObjectHandle v = dict.getKey(key);
    return v.isInteger() ? static_cast<int>(v.getIntValue()) : fallback;
}

// Single-name colour space, or empty for arrays/ICC/indexed we won't touch.
std::string colorSpaceName(QPDFObjectHandle dict) {
    QPDFObjectHandle cs = dict.getKey("/ColorSpace");
    return cs.isName() ? cs.getName() : std::string();
}

// Width (in points) of the largest page that references each image XObject, used
// to estimate the image's effective DPI. Images that appear on no page are absent.
std::map<QPDFObjGen, double> imagePlacementWidths(QPDF& qpdf) {
    std::map<QPDFObjGen, double> widths;
    QPDFPageDocumentHelper dh(qpdf);
    for (QPDFPageObjectHelper& page : dh.getAllPages()) {
        QPDFObjectHandle pageObj = page.getObjectHandle();
        double pageWidth = 612.0; // US Letter fallback
        QPDFObjectHandle mb = pageObj.getKey("/MediaBox");
        if (mb.isArray() && mb.getArrayNItems() == 4) {
            const double x0 = mb.getArrayItem(0).getNumericValue();
            const double x1 = mb.getArrayItem(2).getNumericValue();
            pageWidth = std::abs(x1 - x0);
        }
        QPDFObjectHandle res = pageObj.getKey("/Resources");
        if (!res.isDictionary())
            continue;
        QPDFObjectHandle xobjs = res.getKey("/XObject");
        if (!xobjs.isDictionary())
            continue;
        for (auto const& item : xobjs.getDictAsMap()) {
            QPDFObjectHandle xo = item.second;
            if (!xo.isStream() || !xo.isImage(false))
                continue;
            const QPDFObjGen og = xo.getObjGen();
            auto it = widths.find(og);
            if (it == widths.end() || pageWidth > it->second)
                widths[og] = pageWidth;
        }
    }
    return widths;
}

// Decode, scale to (newW x newH) and re-store an image XObject in place. Returns
// false (leaving the stream untouched) for anything we can't handle safely.
bool downsampleImage(QPDF& qpdf, QPDFObjectHandle image, int newW, int newH) {
    QPDFObjectHandle dict = image.getDict();

    // Stay clear of masks, soft masks and indexed/ICC colour we can't rebuild.
    QPDFObjectHandle imageMask = dict.getKey("/ImageMask");
    if (imageMask.isBool() && imageMask.getBoolValue())
        return false;
    if (dict.hasKey("/SMask") || dict.hasKey("/Mask"))
        return false;

    const int width = intKey(dict, "/Width", 0);
    const int height = intKey(dict, "/Height", 0);
    if (width <= 0 || height <= 0)
        return false;

    const std::vector<std::string> filters = filterNames(dict);

    try {
        // JPEG (DCTDecode): the stored bytes are a JPEG we can hand to QImage.
        if (hasFilter(filters, "/DCTDecode") && filters.size() == 1) {
            std::shared_ptr<Buffer> raw = image.getRawStreamData();
            if (!raw)
                return false;
            QImage img = QImage::fromData(QByteArray(
                reinterpret_cast<const char*>(raw->getBuffer()), static_cast<int>(raw->getSize())));
            if (img.isNull())
                return false;
            QImage scaled = img.scaled(newW, newH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                .convertToFormat(QImage::Format_RGB888);
            QByteArray jpeg;
            QBuffer buf(&jpeg);
            buf.open(QIODevice::WriteOnly);
            if (!scaled.save(&buf, "JPEG", 85))
                return false;
            buf.close();
            image.replaceStreamData(std::string(jpeg.constData(), jpeg.size()),
                                    QPDFObjectHandle::newName("/DCTDecode"),
                                    QPDFObjectHandle::newNull());
            dict.replaceKey("/Width", QPDFObjectHandle::newInteger(scaled.width()));
            dict.replaceKey("/Height", QPDFObjectHandle::newInteger(scaled.height()));
            dict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceRGB"));
            dict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
            dict.removeKey("/DecodeParms");
            return true;
        }

        // Raw samples (uncompressed or generalised filters such as Flate): rebuild
        // a QImage from 8-bit DeviceRGB / DeviceGray samples.
        const int bpc = intKey(dict, "/BitsPerComponent", 8);
        const std::string cs = colorSpaceName(dict);
        int comp = 0;
        QImage::Format fmt = QImage::Format_Invalid;
        if (bpc == 8 && (cs == "/DeviceRGB" || cs == "/CalRGB")) {
            comp = 3;
            fmt = QImage::Format_RGB888;
        } else if (bpc == 8 && (cs == "/DeviceGray" || cs == "/CalGray")) {
            comp = 1;
            fmt = QImage::Format_Grayscale8;
        } else {
            return false;
        }

        std::shared_ptr<Buffer> data = image.getStreamData(qpdf_dl_all);
        if (!data)
            return false;
        const qint64 expected = static_cast<qint64>(width) * height * comp;
        if (static_cast<qint64>(data->getSize()) != expected)
            return false; // predictors / unexpected layout — leave it alone

        QImage src(reinterpret_cast<const uchar*>(data->getBuffer()), width, height, width * comp,
                   fmt);
        QImage scaled = src.copy()
                            .scaled(newW, newH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                            .convertToFormat(fmt);
        if (scaled.isNull())
            return false;

        const int sw = scaled.width();
        const int sh = scaled.height();
        std::string out;
        out.reserve(static_cast<size_t>(sw) * sh * comp);
        for (int y = 0; y < sh; ++y) {
            const char* line = reinterpret_cast<const char*>(scaled.constScanLine(y));
            out.append(line, static_cast<size_t>(sw) * comp);
        }
        // Store uncompressed; QPDFWriter (compress) will Flate-encode it on write.
        image.replaceStreamData(out, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
        dict.replaceKey("/Width", QPDFObjectHandle::newInteger(sw));
        dict.replaceKey("/Height", QPDFObjectHandle::newInteger(sh));
        dict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// Collect the object IDs of embedded font programs (FontFile/2/3), optionally
// stripping them from their descriptors. Returns how many were dropped.
int collectOrDropFontFiles(QPDF& qpdf, std::set<QPDFObjGen>* fontFileIds, bool drop) {
    static const char* kKeys[] = {"/FontFile", "/FontFile2", "/FontFile3"};
    int dropped = 0;
    for (QPDFObjectHandle obj : qpdf.getAllObjects()) {
        if (!obj.isDictionary())
            continue;
        QPDFObjectHandle type = obj.getKey("/Type");
        if (!type.isName() || type.getName() != "/FontDescriptor")
            continue;
        for (const char* key : kKeys) {
            if (!obj.hasKey(key))
                continue;
            if (fontFileIds) {
                QPDFObjectHandle ff = obj.getKey(key);
                if (ff.isStream())
                    fontFileIds->insert(ff.getObjGen());
            }
            if (drop) {
                obj.removeKey(key);
                ++dropped;
            }
        }
    }
    return dropped;
}

} // namespace

bool Optimizer::audit(const QString& inputPath, Audit* out, QString* error) {
    Audit a;
    a.total = QFileInfo(inputPath).size();
    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        std::set<QPDFObjGen> fontFiles;
        collectOrDropFontFiles(qpdf, &fontFiles, /*drop=*/false);

        std::set<QPDFObjGen> contentStreams;
        QPDFPageDocumentHelper dh(qpdf);
        for (QPDFPageObjectHelper& page : dh.getAllPages()) {
            QPDFObjectHandle contents = page.getObjectHandle().getKey("/Contents");
            if (contents.isStream()) {
                contentStreams.insert(contents.getObjGen());
            } else if (contents.isArray()) {
                for (int i = 0; i < contents.getArrayNItems(); ++i) {
                    QPDFObjectHandle c = contents.getArrayItem(i);
                    if (c.isStream())
                        contentStreams.insert(c.getObjGen());
                }
            }
        }

        for (QPDFObjectHandle obj : qpdf.getAllObjects()) {
            if (!obj.isStream())
                continue;
            const qint64 sz = rawStreamSize(obj);
            const QPDFObjGen og = obj.getObjGen();
            QPDFObjectHandle dict = obj.getDict();
            QPDFObjectHandle subtype = dict.getKey("/Subtype");
            QPDFObjectHandle type = dict.getKey("/Type");
            if (obj.isImage(false)) {
                a.images += sz;
            } else if (fontFiles.count(og)) {
                a.fonts += sz;
            } else if (type.isName() && type.getName() == "/Metadata") {
                a.metadata += sz;
            } else if (contentStreams.count(og)) {
                a.content += sz;
            } else {
                a.other += sz;
            }
        }
    } catch (const std::exception& e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return false;
    }

    // Fold any uncategorised on-disk overhead (xref, dictionaries, …) into "other"
    // so the categories add up to the file total.
    const qint64 catTotal = a.images + a.fonts + a.content + a.metadata + a.other;
    if (a.total > catTotal)
        a.other += a.total - catTotal;

    if (out)
        *out = a;
    return true;
}

bool Optimizer::optimize(const QString& inputPath, const QString& outputPath,
                         const Options& options, Report* report, QString* error) {
    Report rep;
    rep.beforeBytes = QFileInfo(inputPath).size();

    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());

        // (b) Downsample images above the target DPI.
        if (options.targetDpi > 0) {
            const std::map<QPDFObjGen, double> placement = imagePlacementWidths(qpdf);
            for (QPDFObjectHandle obj : qpdf.getAllObjects()) {
                if (!obj.isStream() || !obj.isImage(false))
                    continue;
                auto it = placement.find(obj.getObjGen());
                if (it == placement.end())
                    continue; // not placed on any page — can't estimate DPI
                const double widthInches = it->second / 72.0;
                if (widthInches <= 0.0)
                    continue;
                const int pixelW = intKey(obj.getDict(), "/Width", 0);
                const int pixelH = intKey(obj.getDict(), "/Height", 0);
                if (pixelW <= 0 || pixelH <= 0)
                    continue;
                const double effectiveDpi = pixelW / widthInches;
                if (effectiveDpi <= options.targetDpi)
                    continue;
                const double factor = options.targetDpi / effectiveDpi;
                const int newW = qMax(1, static_cast<int>(pixelW * factor + 0.5));
                const int newH = qMax(1, static_cast<int>(pixelH * factor + 0.5));
                if (newW >= pixelW)
                    continue;
                if (downsampleImage(qpdf, obj, newW, newH))
                    ++rep.imagesDownsampled;
                else
                    ++rep.imagesSkipped;
            }
        }

        // (c) Drop embedded font programs (unembed).
        if (options.dropFonts)
            rep.fontsDropped = collectOrDropFontFiles(qpdf, nullptr, /*drop=*/true);

        // (d) Generic stream compression — the classic optimize behaviour.
        QPDFWriter writer(qpdf, target.toLocal8Bit().constData());
        if (options.compress) {
            writer.setObjectStreamMode(qpdf_o_generate); // pack indirect objects together
            writer.setCompressStreams(true);             // compress uncompressed streams
            writer.setRecompressFlate(true);             // recompress flate at the best level
        }
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

    rep.afterBytes = QFileInfo(outputPath).size();
    if (report)
        *report = rep;
    return true;
}
