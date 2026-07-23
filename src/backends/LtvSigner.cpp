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

#include "backends/LtvSigner.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <algorithm>
#include <cstring>
#include <set>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>

namespace {

// ── Small process helpers ───────────────────────────────────────────────────

// Run a tool, optionally feeding `input` on stdin, capturing stdout (binary-safe).
// Returns true on a clean exit 0.
bool run(const QString& prog, const QStringList& args, QByteArray* out,
         const QByteArray& input = QByteArray(), int ms = 30000) {
    QProcess p;
    p.start(prog, args);
    if (!p.waitForStarted(ms))
        return false;
    if (!input.isEmpty())
        p.write(input);
    p.closeWriteChannel();
    if (!p.waitForFinished(ms)) {
        p.kill();
        p.waitForFinished(2000);
        return false;
    }
    if (out)
        *out = p.readAllStandardOutput();
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

// ── DER / PEM helpers ───────────────────────────────────────────────────────

// The total length of the DER structure starting at the front of `d` (header +
// content), read from its ASN.1 length octets. A PDF signature's /Contents is a
// fixed-size placeholder padded with trailing zeros; this trims it back to the
// real signature bytes. Falls back to the whole buffer if it isn't a SEQUENCE.
qint64 derTotalLen(const QByteArray& d) {
    if (d.size() < 2 || static_cast<quint8>(d[0]) != 0x30)
        return d.size();
    const quint8 b1 = static_cast<quint8>(d[1]);
    if (b1 < 0x80)
        return std::min<qint64>(d.size(), 2 + b1);
    const int n = b1 & 0x7F;
    if (n == 0 || n > 4 || d.size() < 2 + n)
        return d.size();
    qint64 len = 0;
    for (int i = 0; i < n; ++i)
        len = (len << 8) | static_cast<quint8>(d[2 + i]);
    return std::min<qint64>(d.size(), 2 + n + len);
}

// Split a PEM blob into its individual base64-wrapped certificate blocks.
QList<QByteArray> splitPemCerts(const QByteArray& pem) {
    QList<QByteArray> out;
    const QByteArray begin = "-----BEGIN CERTIFICATE-----";
    const QByteArray end = "-----END CERTIFICATE-----";
    int from = 0;
    while (true) {
        const int b = pem.indexOf(begin, from);
        if (b < 0)
            break;
        const int e = pem.indexOf(end, b);
        if (e < 0)
            break;
        out << pem.mid(b, e - b + end.size());
        from = e + end.size();
    }
    return out;
}

// The unique certificates carried inside a PKCS#7/CMS signature blob, as DER.
QList<QByteArray> certsFromCms(const QString& openssl, const QByteArray& cms,
                              const QString& workDir) {
    QList<QByteArray> ders;
    const QString cmsPath = workDir + QStringLiteral("/sig.der");
    {
        QFile f(cmsPath);
        if (!f.open(QIODevice::WriteOnly))
            return ders;
        f.write(cms);
    }
    QByteArray pem;
    if (!run(openssl,
             {QStringLiteral("pkcs7"), QStringLiteral("-inform"), QStringLiteral("DER"),
              QStringLiteral("-in"), cmsPath, QStringLiteral("-print_certs"),
              QStringLiteral("-quiet")},
             &pem))
        return ders;
    for (const QByteArray& pemCert : splitPemCerts(pem)) {
        QByteArray der;
        if (run(openssl,
                {QStringLiteral("x509"), QStringLiteral("-inform"), QStringLiteral("PEM"),
                 QStringLiteral("-outform"), QStringLiteral("DER")},
                &der, pemCert)
            && !der.isEmpty())
            ders << der;
    }
    return ders;
}

// A URL captured from a certificate extension (OCSP responder or CRL distribution
// point). Returns the first match or an empty string.
QString extUri(const QString& openssl, const QByteArray& pemCert, const QString& ext,
               const QString& marker) {
    QByteArray text;
    if (!run(openssl,
             {QStringLiteral("x509"), QStringLiteral("-noout"), QStringLiteral("-ext"), ext},
             &text, pemCert))
        return QString();
    static const QRegularExpression uriRe(QStringLiteral("URI:(\\S+)"));
    for (const QString& line : QString::fromUtf8(text).split(QLatin1Char('\n'))) {
        if (!marker.isEmpty() && !line.contains(marker, Qt::CaseInsensitive))
            continue;
        const QRegularExpressionMatch m = uriRe.match(line);
        if (m.hasMatch())
            return m.captured(1).trimmed();
    }
    return QString();
}

QByteArray derToPem(const QString& openssl, const QByteArray& der) {
    QByteArray pem;
    if (run(openssl,
            {QStringLiteral("x509"), QStringLiteral("-inform"), QStringLiteral("DER"),
             QStringLiteral("-outform"), QStringLiteral("PEM")},
            &pem, der))
        return pem;
    return QByteArray();
}

// ── PDF incremental-update writer ────────────────────────────────────────────

struct Obj {
    int id = 0;
    int gen = 0;
    QByteArray serialized; // complete "<id> <gen> obj ... endobj\n"
};

// A raw indirect object: "<id> <gen> obj\n<body>\nendobj\n".
QByteArray makeObj(int id, int gen, const QByteArray& body) {
    QByteArray o = QByteArray::number(id) + ' ' + QByteArray::number(gen) + " obj\n";
    o += body;
    o += "\nendobj\n";
    return o;
}

// A stream object holding `data` verbatim (used for cert/OCSP/CRL DER blobs).
QByteArray makeStreamObj(int id, const QByteArray& data) {
    QByteArray body = "<< /Length " + QByteArray::number(data.size()) + " >>\nstream\n";
    body += data;
    body += "\nendstream";
    return makeObj(id, 0, body);
}

// The byte offset of the file's most recent cross-reference section, read from the
// last "startxref" pointer. -1 if it can't be found.
qint64 lastStartxref(const QByteArray& bytes) {
    const int sx = bytes.lastIndexOf("startxref");
    if (sx < 0)
        return -1;
    int i = sx + 9;
    while (i < bytes.size() && (bytes[i] == '\r' || bytes[i] == '\n' || bytes[i] == ' '))
        ++i;
    qint64 off = 0;
    bool any = false;
    while (i < bytes.size() && bytes[i] >= '0' && bytes[i] <= '9') {
        off = off * 10 + (bytes[i] - '0');
        any = true;
        ++i;
    }
    return any ? off : -1;
}

// Append `objs` (the changed catalog plus the new DSS objects) to `original` as a
// classic incremental update: a fresh xref subsection per run of consecutive ids,
// a trailer chaining back via /Prev, then startxref + %%EOF.
QByteArray writeIncrementalUpdate(const QByteArray& original, qint64 prevXref, QList<Obj> objs,
                                  int rootId, int rootGen, int newSize,
                                  const QByteArray& idArray) {
    QByteArray out = original;
    if (!out.endsWith('\n'))
        out += '\n';

    std::sort(objs.begin(), objs.end(), [](const Obj& a, const Obj& b) { return a.id < b.id; });

    QList<qint64> offsets;
    offsets.reserve(objs.size());
    for (const Obj& o : objs) {
        offsets << out.size();
        out += o.serialized;
    }

    const qint64 xrefPos = out.size();
    QByteArray xref = "xref\n";
    int i = 0;
    while (i < objs.size()) {
        int j = i;
        while (j + 1 < objs.size() && objs[j + 1].id == objs[j].id + 1)
            ++j;
        xref += QByteArray::number(objs[i].id) + ' ' + QByteArray::number(j - i + 1) + '\n';
        for (int k = i; k <= j; ++k) {
            char line[32];
            std::snprintf(line, sizeof(line), "%010lld %05d n\r\n",
                          static_cast<long long>(offsets[k]), objs[k].gen);
            xref += line;
        }
        i = j + 1;
    }
    out += xref;

    QByteArray trailer = "trailer\n<< /Size " + QByteArray::number(newSize) + " /Root "
        + QByteArray::number(rootId) + ' ' + QByteArray::number(rootGen) + " R /Prev "
        + QByteArray::number(prevXref);
    if (!idArray.isEmpty())
        trailer += " /ID " + idArray;
    trailer += " >>\n";
    out += trailer;
    out += "startxref\n" + QByteArray::number(xrefPos) + "\n%%EOF\n";
    return out;
}

// ── Walk the signatures already in the document ──────────────────────────────

// Recursively collect the /Contents byte strings of every signature field.
void collectSignatureContents(QPDFObjectHandle field, QList<QByteArray>* out,
                              std::set<QPDFObjGen>* seen) {
    if (!field.isDictionary())
        return;
    if (field.isIndirect()) {
        const QPDFObjGen og = field.getObjGen();
        if (seen->count(og))
            return;
        seen->insert(og);
    }
    if (field.hasKey("/FT") && field.getKey("/FT").isName()
        && field.getKey("/FT").getName() == "/Sig") {
        QPDFObjectHandle v = field.getKey("/V");
        if (v.isDictionary() && v.hasKey("/Contents")) {
            QPDFObjectHandle c = v.getKey("/Contents");
            if (c.isString()) {
                const std::string s = c.getStringValue();
                out->append(QByteArray(s.data(), static_cast<int>(s.size())));
            }
        }
    }
    if (field.hasKey("/Kids")) {
        QPDFObjectHandle kids = field.getKey("/Kids");
        if (kids.isArray())
            for (int i = 0; i < kids.getArrayNItems(); ++i)
                collectSignatureContents(kids.getArrayItem(i), out, seen);
    }
}

// Serialise a dictionary, substituting `overrides[key]` for those keys and keeping
// every other key's value as-is (indirect references preserved). Keys present in
// `overrides` but not in the dictionary are appended.
QByteArray rebuildDict(QPDFObjectHandle dict, const QMap<QByteArray, QByteArray>& overrides) {
    QByteArray out = "<<";
    QSet<QByteArray> written;
    for (const std::string& k : dict.getKeys()) {
        const QByteArray key = QByteArray::fromStdString(k);
        out += ' ' + key + ' ';
        out += overrides.contains(key) ? overrides.value(key)
                                       : QByteArray::fromStdString(dict.getKey(k).unparse());
        written.insert(key);
    }
    for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it)
        if (!written.contains(it.key()))
            out += ' ' + it.key() + ' ' + it.value();
    out += " >>";
    return out;
}

// "[ a 0 R b 0 R ... ]" for an existing array's items plus one freshly added ref.
QByteArray arrayWithAppended(QPDFObjectHandle array, int newId) {
    QByteArray a = "[";
    if (array.isArray())
        for (int i = 0; i < array.getArrayNItems(); ++i)
            a += ' ' + QByteArray::fromStdString(array.getArrayItem(i).unparse());
    a += ' ' + QByteArray::number(newId) + " 0 R ]";
    return a;
}

} // namespace

bool LtvSigner::addValidationInfo(const QString& inputPath, const QString& outputPath,
                                  QString* error, Result* result) {
    const auto fail = [&](const QString& m) {
        if (error)
            *error = m;
        return false;
    };

    const QString openssl = QStandardPaths::findExecutable(QStringLiteral("openssl"));
    if (openssl.isEmpty())
        return fail(QObject::tr("Long-term validation needs 'openssl' on your PATH."));

    // 1) Read the document structure with QPDF (read-only — the file's bytes, and
    //    therefore every existing signature, are left untouched).
    QPDF pdf;
    int rootId = 0, rootGen = 0, originalSize = 0;
    QByteArray idArray;
    QList<QByteArray> sigContents;
    QByteArray catalogBody;
    try {
        pdf.processFile(inputPath.toLocal8Bit().constData());
        QPDFObjectHandle root = pdf.getRoot();
        rootId = root.getObjGen().getObj();
        rootGen = root.getObjGen().getGen();

        QPDFObjectHandle trailer = pdf.getTrailer();
        if (trailer.hasKey("/Size"))
            originalSize = static_cast<int>(trailer.getKey("/Size").getIntValue());
        if (trailer.hasKey("/ID"))
            idArray = QByteArray::fromStdString(trailer.getKey("/ID").unparseResolved());

        QPDFObjectHandle acro = root.getKey("/AcroForm");
        if (acro.isDictionary() && acro.hasKey("/Fields")) {
            QPDFObjectHandle fields = acro.getKey("/Fields");
            std::set<QPDFObjGen> seen;
            if (fields.isArray())
                for (int i = 0; i < fields.getArrayNItems(); ++i)
                    collectSignatureContents(fields.getArrayItem(i), &sigContents, &seen);
        }
        // unparseResolved() serialises the catalog's own dictionary while leaving
        // its indirect children as references (unparse() would emit just "N G R").
        catalogBody = QByteArray::fromStdString(root.unparseResolved());
    } catch (const std::exception& e) {
        return fail(QObject::tr("The document couldn't be read for signing. %1")
                        .arg(QString::fromUtf8(e.what())));
    }

    if (sigContents.isEmpty())
        return fail(QObject::tr("The document isn't signed yet, so there's nothing to add "
                                "long-term validation for."));

    const QByteArray original = [&] {
        QFile f(inputPath);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }();
    if (original.isEmpty())
        return fail(QObject::tr("The document couldn't be opened."));
    const qint64 prevXref = lastStartxref(original);
    if (prevXref < 0)
        return fail(QObject::tr("The document's cross-reference table couldn't be located."));

    QTemporaryDir tmp;
    if (!tmp.isValid())
        return fail(QObject::tr("Couldn't create a temporary working directory."));

    // 2) Gather validation material per signature: the certificate chain (always)
    //    plus OCSP/CRL responses (best-effort, only when advertised and reachable).
    // Dedup blobs across signatures so a shared chain is stored once.
    QList<QByteArray> certBlobs, ocspBlobs, crlBlobs;
    const auto intern = [](const QByteArray& blob, QList<QByteArray>* list) -> int {
        const int found = list->indexOf(blob);
        if (found >= 0)
            return found;
        list->append(blob);
        return list->size() - 1;
    };

    struct Vri {
        QByteArray hashKey; // uppercase hex SHA-1 of the signature contents
        QList<int> certIdx, ocspIdx, crlIdx;
    };
    QList<Vri> vris;

    for (const QByteArray& contents : sigContents) {
        const QByteArray real = contents.left(derTotalLen(contents));
        Vri vri;
        vri.hashKey = QCryptographicHash::hash(real, QCryptographicHash::Sha1).toHex().toUpper();

        const QList<QByteArray> chain = certsFromCms(openssl, real, tmp.path());
        for (const QByteArray& der : chain)
            vri.certIdx << intern(der, &certBlobs);

        // OCSP needs the certificate and its issuer; the chain is signer-first, so
        // pair each cert with the next one as a candidate issuer.
        for (int i = 0; i < chain.size(); ++i) {
            const QByteArray pem = derToPem(openssl, chain[i]);
            if (pem.isEmpty())
                continue;

            const QString crlUrl =
                extUri(openssl, pem, QStringLiteral("crlDistributionPoints"), QString());
            if (!crlUrl.isEmpty()) {
                const QString crlDer = tmp.path() + QStringLiteral("/crl.der");
                QByteArray dl;
                const QString curl = QStandardPaths::findExecutable(QStringLiteral("curl"));
                if (!curl.isEmpty()
                    && run(curl,
                           {QStringLiteral("-fsS"), QStringLiteral("--max-time"),
                            QStringLiteral("20"), QStringLiteral("-o"), crlDer, crlUrl},
                           nullptr)) {
                    QFile cf(crlDer);
                    if (cf.open(QIODevice::ReadOnly)) {
                        QByteArray data = cf.readAll();
                        // A CRL fetched over HTTP is usually DER; convert if it's PEM.
                        if (data.startsWith("-----BEGIN")) {
                            QByteArray der;
                            if (run(openssl,
                                    {QStringLiteral("crl"), QStringLiteral("-inform"),
                                     QStringLiteral("PEM"), QStringLiteral("-outform"),
                                     QStringLiteral("DER")},
                                    &der, data))
                                data = der;
                        }
                        if (!data.isEmpty())
                            vri.crlIdx << intern(data, &crlBlobs);
                    }
                }
            }

            const QString ocspUrl =
                extUri(openssl, pem, QStringLiteral("authorityInfoAccess"), QStringLiteral("OCSP"));
            if (!ocspUrl.isEmpty() && i + 1 < chain.size()) {
                const QString issuerPem = tmp.path() + QStringLiteral("/issuer.pem");
                const QString certPem = tmp.path() + QStringLiteral("/cert.pem");
                {
                    QFile f(issuerPem);
                    if (f.open(QIODevice::WriteOnly))
                        f.write(derToPem(openssl, chain[i + 1]));
                    QFile g(certPem);
                    if (g.open(QIODevice::WriteOnly))
                        g.write(pem);
                }
                const QString respDer = tmp.path() + QStringLiteral("/ocsp.der");
                QByteArray ignore;
                if (run(openssl,
                        {QStringLiteral("ocsp"), QStringLiteral("-issuer"), issuerPem,
                         QStringLiteral("-cert"), certPem, QStringLiteral("-url"), ocspUrl,
                         QStringLiteral("-no_nonce"), QStringLiteral("-noverify"),
                         QStringLiteral("-respout"), respDer, QStringLiteral("-timeout"),
                         QStringLiteral("20")},
                        &ignore)) {
                    QFile rf(respDer);
                    if (rf.open(QIODevice::ReadOnly)) {
                        const QByteArray data = rf.readAll();
                        if (!data.isEmpty())
                            vri.ocspIdx << intern(data, &ocspBlobs);
                    }
                }
            }
        }
        vris << vri;
    }

    if (certBlobs.isEmpty())
        return fail(QObject::tr("No certificates could be read from the document's signatures."));

    // 3) Allocate object ids for the new DSS objects (catalog keeps its own id).
    int nextId = std::max(originalSize, std::max(rootId + 1, 1));
    QList<Obj> objs;
    const auto refArray = [](const QList<int>& ids) {
        QByteArray a = "[";
        for (int id : ids)
            a += ' ' + QByteArray::number(id) + " 0 R";
        a += " ]";
        return a;
    };

    QList<int> certObjIds, ocspObjIds, crlObjIds;
    for (const QByteArray& der : certBlobs) {
        const int id = nextId++;
        certObjIds << id;
        objs << Obj{id, 0, makeStreamObj(id, der)};
    }
    for (const QByteArray& der : ocspBlobs) {
        const int id = nextId++;
        ocspObjIds << id;
        objs << Obj{id, 0, makeStreamObj(id, der)};
    }
    for (const QByteArray& der : crlBlobs) {
        const int id = nextId++;
        crlObjIds << id;
        objs << Obj{id, 0, makeStreamObj(id, der)};
    }

    // VRI dictionary: one entry per signature, keyed by the hash of its contents.
    const int vriId = nextId++;
    {
        QByteArray body = "<<";
        for (const Vri& v : vris) {
            const auto mapIds = [](const QList<int>& localIdx, const QList<int>& objIds) {
                QList<int> r;
                for (int i : localIdx)
                    r << objIds[i];
                return r;
            };
            body += " /" + v.hashKey + " <<";
            if (!v.certIdx.isEmpty())
                body += " /Cert " + refArray(mapIds(v.certIdx, certObjIds));
            if (!v.ocspIdx.isEmpty())
                body += " /OCSP " + refArray(mapIds(v.ocspIdx, ocspObjIds));
            if (!v.crlIdx.isEmpty())
                body += " /CRL " + refArray(mapIds(v.crlIdx, crlObjIds));
            body += " >>";
        }
        body += " >>";
        objs << Obj{vriId, 0, makeObj(vriId, 0, body)};
    }

    // The DSS dictionary itself.
    const int dssId = nextId++;
    {
        QByteArray body = "<< /Type /DSS /Certs " + refArray(certObjIds);
        if (!ocspObjIds.isEmpty())
            body += " /OCSPs " + refArray(ocspObjIds);
        if (!crlObjIds.isEmpty())
            body += " /CRLs " + refArray(crlObjIds);
        body += " /VRI " + QByteArray::number(vriId) + " 0 R >>";
        objs << Obj{dssId, 0, makeObj(dssId, 0, body)};
    }

    // 4) Rewrite the catalog so /DSS points at our new store (dropping any prior one).
    {
        static const QRegularExpression dssRe(QStringLiteral("/DSS\\s+\\d+\\s+\\d+\\s+R\\s*"));
        QString cat = QString::fromLatin1(catalogBody);
        cat.remove(dssRe);
        const int open = cat.indexOf(QLatin1String("<<"));
        if (open < 0)
            return fail(QObject::tr("The document catalog couldn't be updated."));
        cat.insert(open + 2, QStringLiteral(" /DSS %1 0 R").arg(dssId));
        objs << Obj{rootId, rootGen, makeObj(rootId, rootGen, cat.toLatin1())};
    }

    const int newSize = nextId;
    const QByteArray updated =
        writeIncrementalUpdate(original, prevXref, objs, rootId, rootGen, newSize, idArray);

    QFile out(outputPath);
    if (!out.open(QIODevice::WriteOnly))
        return fail(QObject::tr("The result couldn't be written."));
    if (out.write(updated) != updated.size())
        return fail(QObject::tr("The result couldn't be fully written."));
    out.close();

    if (result) {
        result->signatures = sigContents.size();
        result->certs = certBlobs.size();
        result->ocsps = ocspBlobs.size();
        result->crls = crlBlobs.size();
    }
    return true;
}

bool LtvSigner::addDocumentTimestamp(const QString& inputPath, const QString& outputPath,
                                     const QString& tsaUrl, QString* error) {
    const auto fail = [&](const QString& m) {
        if (error)
            *error = m;
        return false;
    };

    const QString openssl = QStandardPaths::findExecutable(QStringLiteral("openssl"));
    const QString curl = QStandardPaths::findExecutable(QStringLiteral("curl"));
    if (openssl.isEmpty() || curl.isEmpty())
        return fail(QObject::tr("Archive timestamping needs 'openssl' and 'curl' on your PATH."));
    if (tsaUrl.isEmpty())
        return fail(QObject::tr("No timestamp authority (TSA) URL was given."));

    // 1) Read the structure we have to extend: the AcroForm (to register the
    //    timestamp field) and a page (to carry its invisible widget).
    QPDF pdf;
    int rootId = 0, rootGen = 0, originalSize = 0;
    QByteArray idArray;
    QList<Obj> objs;
    int nextId = 0;
    int dtsId = 0; // the /DocTimeStamp value dictionary
    try {
        pdf.processFile(inputPath.toLocal8Bit().constData());
        QPDFObjectHandle root = pdf.getRoot();
        rootId = root.getObjGen().getObj();
        rootGen = root.getObjGen().getGen();

        QPDFObjectHandle trailer = pdf.getTrailer();
        if (trailer.hasKey("/Size"))
            originalSize = static_cast<int>(trailer.getKey("/Size").getIntValue());
        if (trailer.hasKey("/ID"))
            idArray = QByteArray::fromStdString(trailer.getKey("/ID").unparseResolved());

        QPDFObjectHandle acro = root.getKey("/AcroForm");
        if (!acro.isDictionary() || !acro.isIndirect())
            return fail(QObject::tr("The document has no signature form to attach a timestamp to."));

        std::vector<QPDFPageObjectHelper> pages = QPDFPageDocumentHelper(pdf).getAllPages();
        if (pages.empty())
            return fail(QObject::tr("The document has no pages."));
        QPDFObjectHandle page = pages.front().getObjectHandle();
        if (!page.isIndirect())
            return fail(QObject::tr("The document's first page couldn't be addressed."));

        nextId = std::max(originalSize, rootId + 1);
        dtsId = nextId++;
        const int widgetId = nextId++;

        // The invisible widget/field that owns the timestamp signature value.
        const QByteArray widgetName =
            "(FeatherDocTS" + QByteArray::number(QDateTime::currentMSecsSinceEpoch()) + ")";
        QByteArray widget = "<< /Type /Annot /Subtype /Widget /FT /Sig /F 2 "
                            "/Rect [0 0 0 0] /T "
            + widgetName + " /P " + QByteArray::number(page.getObjGen().getObj()) + " 0 R /V "
            + QByteArray::number(dtsId) + " 0 R >>";
        objs << Obj{widgetId, 0, makeObj(widgetId, 0, widget)};

        // The /DocTimeStamp value, with fixed-width placeholders we patch after the
        // file is assembled: /ByteRange numbers (three 10-char fields) and a large
        // /Contents slot for the RFC 3161 token (zeros = valid hex padding).
        const QByteArray brSlots(32, ' ');
        const QByteArray contents(2 * 16384, '0');
        QByteArray dts = "<< /Type /DocTimeStamp /Filter /Adobe.PPKLite /SubFilter /ETSI.RFC3161 "
                         "/ByteRange [0 "
            + brSlots + "] /Contents <" + contents + "> >>";
        objs << Obj{dtsId, 0, makeObj(dtsId, 0, dts)};

        // Register the widget in the AcroForm /Fields, and make sure /SigFlags marks
        // the document as append-only-with-signatures.
        QPDFObjectHandle fields = acro.getKey("/Fields");
        if (fields.isIndirect()) {
            objs << Obj{fields.getObjGen().getObj(), fields.getObjGen().getGen(),
                        makeObj(fields.getObjGen().getObj(), fields.getObjGen().getGen(),
                                arrayWithAppended(fields, widgetId))};
            QMap<QByteArray, QByteArray> ov;
            ov["/SigFlags"] = "3";
            objs << Obj{acro.getObjGen().getObj(), acro.getObjGen().getGen(),
                        makeObj(acro.getObjGen().getObj(), acro.getObjGen().getGen(),
                                rebuildDict(acro, ov))};
        } else {
            QMap<QByteArray, QByteArray> ov;
            ov["/Fields"] = arrayWithAppended(fields, widgetId);
            ov["/SigFlags"] = "3";
            objs << Obj{acro.getObjGen().getObj(), acro.getObjGen().getGen(),
                        makeObj(acro.getObjGen().getObj(), acro.getObjGen().getGen(),
                                rebuildDict(acro, ov))};
        }

        // Add the widget to the first page's /Annots.
        QPDFObjectHandle annots = page.getKey("/Annots");
        if (annots.isArray() && annots.isIndirect()) {
            objs << Obj{annots.getObjGen().getObj(), annots.getObjGen().getGen(),
                        makeObj(annots.getObjGen().getObj(), annots.getObjGen().getGen(),
                                arrayWithAppended(annots, widgetId))};
        } else {
            QMap<QByteArray, QByteArray> ov;
            ov["/Annots"] = arrayWithAppended(annots, widgetId);
            objs << Obj{page.getObjGen().getObj(), page.getObjGen().getGen(),
                        makeObj(page.getObjGen().getObj(), page.getObjGen().getGen(),
                                rebuildDict(page, ov))};
        }
    } catch (const std::exception& e) {
        return fail(QObject::tr("The document couldn't be read for timestamping. %1")
                        .arg(QString::fromUtf8(e.what())));
    }

    const QByteArray original = [&] {
        QFile f(inputPath);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }();
    if (original.isEmpty())
        return fail(QObject::tr("The document couldn't be opened."));
    const qint64 prevXref = lastStartxref(original);
    if (prevXref < 0)
        return fail(QObject::tr("The document's cross-reference table couldn't be located."));

    const int newSize = std::max(originalSize, nextId);
    QByteArray bytes =
        writeIncrementalUpdate(original, prevXref, objs, rootId, rootGen, newSize, idArray);

    // 2) Locate the placeholders we just wrote (only within the appended region, so
    //    the document's existing signature isn't mistaken for ours).
    const int from = original.size();
    const QByteArray brTag = "/ByteRange [0 ";
    const int brAt = bytes.indexOf(brTag, from);
    const int contAt = bytes.indexOf("/Contents <", from);
    if (brAt < 0 || contAt < 0)
        return fail(QObject::tr("The timestamp placeholder couldn't be located."));
    const int slot = brAt + brTag.size();      // first byte of the three ByteRange fields
    const int gapStart = contAt + 10;          // the '<' of /Contents <...>
    const int gapEnd = bytes.indexOf('>', gapStart);
    if (gapEnd < 0)
        return fail(QObject::tr("The timestamp placeholder couldn't be located."));

    // 3) Fill the ByteRange first (its bytes are part of what gets signed), then
    //    hash everything except the contents gap.
    const qint64 a = gapStart;                 // length of the first signed segment
    const qint64 b = gapEnd + 1;               // start of the second signed segment
    const qint64 c = bytes.size() - (gapEnd + 1);
    char br[40];
    std::snprintf(br, sizeof(br), "%10lld %10lld %10lld", static_cast<long long>(a),
                  static_cast<long long>(b), static_cast<long long>(c));
    std::memcpy(bytes.data() + slot, br, 32);

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArrayView(bytes).first(gapStart));
    hash.addData(QByteArrayView(bytes).sliced(gapEnd + 1));
    const QByteArray digestHex = hash.result().toHex();

    // 4) Ask the TSA to sign that digest, and pull the bare timestamp token out of
    //    its reply (ETSI.RFC3161 wants the token, not the full response).
    QTemporaryDir tmp;
    if (!tmp.isValid())
        return fail(QObject::tr("Couldn't create a temporary working directory."));
    const QString tsq = tmp.filePath(QStringLiteral("req.tsq"));
    const QString tsr = tmp.filePath(QStringLiteral("resp.tsr"));
    const QString tok = tmp.filePath(QStringLiteral("token.der"));

    QByteArray ignore;
    if (!run(openssl,
             {QStringLiteral("ts"), QStringLiteral("-query"), QStringLiteral("-digest"),
              QString::fromLatin1(digestHex), QStringLiteral("-sha256"), QStringLiteral("-cert"),
              QStringLiteral("-out"), tsq},
             &ignore))
        return fail(QObject::tr("Couldn't build the timestamp request."));
    if (!run(curl,
             {QStringLiteral("-fsS"), QStringLiteral("--max-time"), QStringLiteral("25"),
              QStringLiteral("-H"), QStringLiteral("Content-Type: application/timestamp-query"),
              QStringLiteral("--data-binary"), QStringLiteral("@") + tsq, tsaUrl,
              QStringLiteral("-o"), tsr},
             &ignore))
        return fail(QObject::tr("Couldn't reach the timestamp authority. Check the TSA URL and "
                                "your connection."));
    if (!run(openssl,
             {QStringLiteral("ts"), QStringLiteral("-reply"), QStringLiteral("-in"), tsr,
              QStringLiteral("-token_out"), QStringLiteral("-out"), tok},
             &ignore))
        return fail(QObject::tr("The timestamp authority's reply wasn't a valid RFC 3161 token."));

    QByteArray token;
    {
        QFile f(tok);
        if (f.open(QIODevice::ReadOnly))
            token = f.readAll();
    }
    if (token.isEmpty())
        return fail(QObject::tr("The timestamp token was empty."));
    const QByteArray tokenHex = token.toHex();
    if (tokenHex.size() > gapEnd - (gapStart + 1))
        return fail(QObject::tr("The timestamp token is larger than the reserved space."));

    // 5) Drop the token into the contents gap (the rest stays zero padding). This
    //    region is excluded from the byte range, so the digest stays valid.
    std::memcpy(bytes.data() + gapStart + 1, tokenHex.constData(), tokenHex.size());

    QFile out(outputPath);
    if (!out.open(QIODevice::WriteOnly))
        return fail(QObject::tr("The result couldn't be written."));
    if (out.write(bytes) != bytes.size())
        return fail(QObject::tr("The result couldn't be fully written."));
    return true;
}
