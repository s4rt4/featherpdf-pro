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

#include "backends/PdfA.h"

#include <QColorSpace>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFWriter.hh>

#include <string>

namespace {
// XML-escape a string for inclusion in the XMP packet.
QString xmlEscape(const QString& in) {
    QString out;
    out.reserve(in.size());
    for (const QChar c : in) {
        switch (c.unicode()) {
        case '&': out += QStringLiteral("&amp;"); break;
        case '<': out += QStringLiteral("&lt;"); break;
        case '>': out += QStringLiteral("&gt;"); break;
        case '"': out += QStringLiteral("&quot;"); break;
        case '\'': out += QStringLiteral("&apos;"); break;
        default: out += c; break;
        }
    }
    return out;
}

// Pull a readable string out of an Info-dict text value, if present.
QString infoString(QPDFObjectHandle info, const char* key) {
    if (!info.isDictionary() || !info.hasKey(key))
        return {};
    QPDFObjectHandle v = info.getKey(key);
    if (!v.isString())
        return {};
    return QString::fromStdString(v.getUTF8Value());
}

// Build a minimal, well-formed XMP packet declaring PDF/A-1b conformance plus
// Dublin Core / XMP basics.
QByteArray buildXmp(const QString& title, const QString& author) {
    const QString safeTitle = xmlEscape(title.isEmpty() ? QStringLiteral("Untitled") : title);
    const QString safeAuthor = xmlEscape(author.isEmpty() ? QStringLiteral("Unknown") : author);
    const QString xmp = QStringLiteral(
        "<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n"
        "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">\n"
        " <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
        "  <rdf:Description rdf:about=\"\"\n"
        "      xmlns:pdfaid=\"http://www.aiim.org/pdfa/ns/id/\">\n"
        "   <pdfaid:part>1</pdfaid:part>\n"
        "   <pdfaid:conformance>B</pdfaid:conformance>\n"
        "  </rdf:Description>\n"
        "  <rdf:Description rdf:about=\"\"\n"
        "      xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
        "   <dc:format>application/pdf</dc:format>\n"
        "   <dc:title><rdf:Alt><rdf:li xml:lang=\"x-default\">%1</rdf:li></rdf:Alt></dc:title>\n"
        "   <dc:creator><rdf:Seq><rdf:li>%2</rdf:li></rdf:Seq></dc:creator>\n"
        "  </rdf:Description>\n"
        "  <rdf:Description rdf:about=\"\"\n"
        "      xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\">\n"
        "   <xmp:CreatorTool>Feather PDF</xmp:CreatorTool>\n"
        "  </rdf:Description>\n"
        " </rdf:RDF>\n"
        "</x:xmpmeta>\n"
        "<?xpacket end=\"w\"?>\n")
        .arg(safeTitle, safeAuthor);
    return xmp.toUtf8();
}

std::string toStd(const QByteArray& b) {
    return std::string(b.constData(), static_cast<size_t>(b.size()));
}
}

bool PdfA::convertToPdfA1b(const QString& inputPath, const QString& outputPath, QString* error) {
    const bool inPlace =
        QFileInfo(inputPath).absoluteFilePath() == QFileInfo(outputPath).absoluteFilePath();
    const QString target = inPlace ? outputPath + QStringLiteral(".feather-tmp") : outputPath;

    try {
        QPDF qpdf;
        qpdf.processFile(inputPath.toLocal8Bit().constData());
        QPDFObjectHandle root = qpdf.getRoot();

        // Title / author for the XMP, reusing any existing Info dict values.
        QPDFObjectHandle info = qpdf.getTrailer().getKey("/Info");
        const QString title = infoString(info, "/Title");
        const QString author = infoString(info, "/Author");

        // --- sRGB OutputIntent (GTS_PDFA1) -----------------------------------
        // Qt synthesises a standard sRGB ICC profile for us, so we don't have to
        // ship one - lawful and small. PDF/A-1 needs exactly one OutputIntent
        // with a GTS_PDFA1 subtype and an embedded destination profile.
        const QByteArray icc = QColorSpace(QColorSpace::SRgb).iccProfile();
        if (icc.isEmpty())
            throw std::runtime_error("Couldn't generate an sRGB ICC profile.");

        QPDFObjectHandle iccStream = QPDFObjectHandle::newStream(&qpdf, toStd(icc));
        QPDFObjectHandle iccDict = iccStream.getDict();
        iccDict.replaceKey("/N", QPDFObjectHandle::newInteger(3)); // RGB → 3 components
        iccDict.replaceKey("/Alternate", QPDFObjectHandle::newName("/DeviceRGB"));

        QPDFObjectHandle oi = QPDFObjectHandle::newDictionary();
        oi.replaceKey("/Type", QPDFObjectHandle::newName("/OutputIntent"));
        oi.replaceKey("/S", QPDFObjectHandle::newName("/GTS_PDFA1"));
        oi.replaceKey("/OutputConditionIdentifier", QPDFObjectHandle::newString("sRGB"));
        oi.replaceKey("/OutputCondition", QPDFObjectHandle::newString("sRGB IEC61966-2.1"));
        oi.replaceKey("/Info", QPDFObjectHandle::newString("sRGB IEC61966-2.1"));
        oi.replaceKey("/RegistryName", QPDFObjectHandle::newString("http://www.color.org"));
        oi.replaceKey("/DestOutputProfile", iccStream);

        QPDFObjectHandle outputIntents = QPDFObjectHandle::newArray();
        outputIntents.appendItem(oi);
        root.replaceKey("/OutputIntents", outputIntents);

        // --- XMP metadata packet ---------------------------------------------
        QPDFObjectHandle meta = QPDFObjectHandle::newStream(&qpdf, toStd(buildXmp(title, author)));
        QPDFObjectHandle metaDict = meta.getDict();
        metaDict.replaceKey("/Type", QPDFObjectHandle::newName("/Metadata"));
        metaDict.replaceKey("/Subtype", QPDFObjectHandle::newName("/XML"));
        root.replaceKey("/Metadata", meta);

        // --- MarkInfo --------------------------------------------------------
        QPDFObjectHandle markInfo = QPDFObjectHandle::newDictionary();
        markInfo.replaceKey("/Marked", QPDFObjectHandle::newBool(true));
        root.replaceKey("/MarkInfo", markInfo);

        QPDFWriter writer(qpdf, target.toLocal8Bit().constData());
        // A stable, content-derived file /ID (PDF/A wants the trailer /ID set).
        writer.setDeterministicID(true);
        // The XMP must stay readable, so keep the metadata stream uncompressed.
        writer.setStreamDataMode(qpdf_s_preserve);
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
                *error = QStringLiteral("The PDF/A copy couldn't replace the original.");
            QFile::remove(target);
            return false;
        }
    }
    return true;
}

bool PdfA::hasValidator() {
    return !QStandardPaths::findExecutable(QStringLiteral("verapdf")).isEmpty();
}

bool PdfA::validate(const QString& inputPath, PreflightReport* report, QString* error) {
    PreflightReport rep;
    rep.profile = QStringLiteral("PDF/A-1B");

    const QString tool = QStandardPaths::findExecutable(QStringLiteral("verapdf"));
    if (tool.isEmpty()) {
        // Graceful absence: not an error, just an unavailable verdict.
        rep.available = false;
        rep.summary = QStringLiteral("veraPDF is not installed, so the file couldn't be validated.");
        if (report)
            *report = rep;
        return true;
    }
    rep.available = true;

    QProcess proc;
    // -f 1b: validate against PDF/A-1b; --format text -v: a parseable verdict
    // with the failed rule clauses listed underneath.
    proc.start(tool, {QStringLiteral("-f"), QStringLiteral("1b"), QStringLiteral("--format"),
                      QStringLiteral("text"), QStringLiteral("-v"), inputPath});
    if (!proc.waitForStarted(15000)) {
        if (error)
            *error = QStringLiteral("veraPDF couldn't be started.");
        return false;
    }
    if (!proc.waitForFinished(120000) || proc.exitStatus() != QProcess::NormalExit) {
        if (error)
            *error = QStringLiteral("veraPDF didn't finish cleanly.");
        return false;
    }

    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    bool sawVerdict = false;
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;
        if (!sawVerdict && (line.startsWith(QLatin1String("PASS")) ||
                            line.startsWith(QLatin1String("FAIL")))) {
            // First line is the file's overall verdict: "PASS <file> 1b".
            rep.pass = line.startsWith(QLatin1String("PASS"));
            rep.ran = true;
            sawVerdict = true;
            continue;
        }
        // Indented "FAIL <rule>" lines are the individual violations.
        if (sawVerdict && line.startsWith(QLatin1String("FAIL")) && rep.violations.size() < 20)
            rep.violations.append(line.mid(4).trimmed());
    }

    if (!rep.ran) {
        // No recognisable verdict line - report whatever verapdf printed.
        rep.summary = out.trimmed().isEmpty()
                          ? QStringLiteral("veraPDF produced no readable verdict.")
                          : out.trimmed();
    } else if (rep.pass) {
        rep.summary = QStringLiteral("Valid PDF/A-1b: the file conforms.");
    } else {
        rep.summary = rep.violations.isEmpty()
                          ? QStringLiteral("Not valid PDF/A-1b.")
                          : QStringLiteral("Not valid PDF/A-1b: %n issue(s) found.").replace(
                                QStringLiteral("%n"), QString::number(rep.violations.size()));
    }

    if (report)
        *report = rep;
    return true;
}
