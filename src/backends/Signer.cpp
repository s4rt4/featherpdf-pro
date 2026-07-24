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

#include "backends/Signer.h"

#include "backends/PdfIncrement.h"
#include "backends/ToolLocator.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QObject>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVarLengthArray>

#include <algorithm>
#include <cstring>
#include <functional>
#include <set>
#include <vector>

#define NOMINMAX
#include <windows.h>

#include <wincrypt.h>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>

using PdfIncrement::Obj;
using PdfIncrement::arrayWithAppended;
using PdfIncrement::lastStartxref;
using PdfIncrement::makeObj;
using PdfIncrement::makeStreamObj;
using PdfIncrement::rebuildDict;
using PdfIncrement::writeIncrementalUpdate;

namespace {

// ── Windows certificate store ────────────────────────────────────────────────

QDateTime fromFileTime(const FILETIME& ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    // 100ns ticks since 1601-01-01 → ms since the Unix epoch.
    return QDateTime::fromMSecsSinceEpoch(
        qint64(u.QuadPart / 10000) - qint64(11644473600000LL));
}

QString certCommonName(PCCERT_CONTEXT cert) {
    const DWORD len =
        CertGetNameStringW(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nullptr, 0);
    if (len <= 1)
        return QString();
    std::vector<wchar_t> buf(len);
    CertGetNameStringW(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, buf.data(), len);
    return QString::fromWCharArray(buf.data());
}

bool certHasPrivateKey(PCCERT_CONTEXT cert) {
    DWORD cb = 0;
    return CertGetCertificateContextProperty(cert, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &cb)
        || CertGetCertificateContextProperty(cert, CERT_NCRYPT_KEY_HANDLE_PROP_ID, nullptr, &cb);
}

// The name shown in the certificate combo box. The expiry makes same-CN
// renewals distinguishable, and sign() rebuilds the same string to resolve the
// user's pick back to a store entry.
QString certDisplay(PCCERT_CONTEXT cert) {
    return QStringLiteral("%1 (%2)").arg(
        certCommonName(cert),
        fromFileTime(cert->pCertInfo->NotAfter).date().toString(Qt::ISODate));
}

// Visit every time-valid certificate with a private key in the user's "MY"
// store. `visit` returns true to keep the certificate (enumeration stops and
// the context is duplicated for the caller).
PCCERT_CONTEXT eachSigningCert(const std::function<bool(PCCERT_CONTEXT)>& visit) {
    HCERTSTORE store = CertOpenStore(
        CERT_STORE_PROV_SYSTEM_W, 0, 0,
        CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_READONLY_FLAG | CERT_STORE_OPEN_EXISTING_FLAG,
        L"MY");
    if (!store)
        return nullptr;
    PCCERT_CONTEXT found = nullptr;
    PCCERT_CONTEXT c = nullptr;
    while ((c = CertEnumCertificatesInStore(store, c)) != nullptr) {
        if (!certHasPrivateKey(c))
            continue;
        if (CertVerifyTimeValidity(nullptr, c->pCertInfo) != 0)
            continue;
        if (visit(c)) {
            found = CertDuplicateCertificateContext(c);
            CertFreeCertificateContext(c);
            break;
        }
    }
    CertCloseStore(store, 0);
    return found;
}

// ── PDF strings and dates ────────────────────────────────────────────────────

// Any text as a PDF hex string in UTF-16BE with BOM — safe for every script.
QByteArray pdfTextString(const QString& s) {
    QByteArray hex = "<FEFF";
    for (const QChar ch : s) {
        char b[5];
        std::snprintf(b, sizeof(b), "%04X", ch.unicode());
        hex += b;
    }
    hex += '>';
    return hex;
}

// Escape a string for use inside a content-stream literal ( ... ) Tj.
QByteArray contentText(const QString& s) {
    const QByteArray latin = s.toLatin1(); // Helvetica/WinAnsi: non-Latin-1 → '?'
    QByteArray out;
    for (const char c : latin) {
        if (c == '(' || c == ')' || c == '\\')
            out += '\\';
        out += c;
    }
    return out;
}

QByteArray pdfDate(const QDateTime& dt) {
    const int off = dt.offsetFromUtc();
    const char sign = off < 0 ? '-' : '+';
    const int a = std::abs(off);
    return "(D:" + dt.toString(QStringLiteral("yyyyMMddHHmmss")).toLatin1() + sign
        + QByteArray::number(a / 3600).rightJustified(2, '0') + '\''
        + QByteArray::number((a % 3600) / 60).rightJustified(2, '0') + "')";
}

QByteArray num(double v) {
    return QByteArray::number(v, 'f', 2);
}

// The total length of the DER structure at the front of `d` — trims a /Contents
// placeholder's zero padding back to the real CMS bytes.
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

// Decode a PDF text string: UTF-16BE when it carries a BOM, else Latin-1.
QString pdfStringDecode(const QByteArray& raw) {
    if (raw.size() >= 2 && static_cast<quint8>(raw[0]) == 0xFE
        && static_cast<quint8>(raw[1]) == 0xFF) {
        QString out;
        for (int i = 2; i + 1 < raw.size(); i += 2)
            out += QChar((static_cast<quint8>(raw[i]) << 8) | static_cast<quint8>(raw[i + 1]));
        return out;
    }
    return QString::fromLatin1(raw);
}

// ── CMS (PKCS#7) sign / verify ───────────────────────────────────────────────

QByteArray cmsSignDetached(PCCERT_CONTEXT cert, const QByteArray& bytes, qint64 gapStart,
                           qint64 tailStart, QString* error) {
    // Include the certificate chain so verifiers (and LTV) can rebuild it.
    QVarLengthArray<PCCERT_CONTEXT, 8> msgCerts;
    PCCERT_CHAIN_CONTEXT chain = nullptr;
    CERT_CHAIN_PARA chainPara{};
    chainPara.cbSize = sizeof(chainPara);
    if (CertGetCertificateChain(nullptr, cert, nullptr, nullptr, &chainPara, 0, nullptr, &chain)
        && chain->cChain > 0) {
        const CERT_SIMPLE_CHAIN* sc = chain->rgpChain[0];
        for (DWORD i = 0; i < sc->cElement; ++i)
            msgCerts.append(sc->rgpElement[i]->pCertContext);
    } else {
        msgCerts.append(cert);
    }

    CRYPT_SIGN_MESSAGE_PARA para{};
    para.cbSize = sizeof(para);
    para.dwMsgEncodingType = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;
    para.pSigningCert = cert;
    para.HashAlgorithm.pszObjId = const_cast<char*>(szOID_NIST_sha256);
    para.cMsgCert = DWORD(msgCerts.size());
    para.rgpMsgCert = msgCerts.data();

    const BYTE* parts[2] = {
        reinterpret_cast<const BYTE*>(bytes.constData()),
        reinterpret_cast<const BYTE*>(bytes.constData()) + tailStart,
    };
    DWORD sizes[2] = {DWORD(gapStart), DWORD(bytes.size() - tailStart)};

    QByteArray cms;
    DWORD cb = 0;
    if (CryptSignMessage(&para, TRUE, 2, parts, sizes, nullptr, &cb) && cb > 0) {
        cms.resize(int(cb));
        if (!CryptSignMessage(&para, TRUE, 2, parts, sizes,
                              reinterpret_cast<BYTE*>(cms.data()), &cb)) {
            cms.clear();
        } else {
            cms.truncate(int(cb));
        }
    }
    if (chain)
        CertFreeCertificateChain(chain);
    if (cms.isEmpty() && error) {
        *error = QObject::tr("Windows couldn't create the signature (error 0x%1). If the key is "
                             "on a smartcard, make sure it is inserted.")
                     .arg(quint32(GetLastError()), 8, 16, QLatin1Char('0'));
    }
    return cms;
}

// One signature field's parsed pieces.
struct SigEntry {
    QByteArray contents;
    QList<qint64> ranges; // offset,len pairs
    QString name, reason, location, mDate, subFilter;
};

void collectSignatures(QPDFObjectHandle field, QList<SigEntry>* out, std::set<QPDFObjGen>* seen) {
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
        if (v.isDictionary() && v.hasKey("/Contents") && v.hasKey("/ByteRange")) {
            SigEntry e;
            const std::string s = v.getKey("/Contents").getStringValue();
            e.contents = QByteArray(s.data(), int(s.size()));
            QPDFObjectHandle br = v.getKey("/ByteRange");
            if (br.isArray())
                for (int i = 0; i < br.getArrayNItems(); ++i)
                    if (br.getArrayItem(i).isInteger())
                        e.ranges << br.getArrayItem(i).getIntValue();
            const auto text = [&](const char* key) {
                if (!v.hasKey(key) || !v.getKey(key).isString())
                    return QString();
                const std::string t = v.getKey(key).getStringValue();
                return pdfStringDecode(QByteArray(t.data(), int(t.size())));
            };
            e.name = text("/Name");
            e.reason = text("/Reason");
            e.location = text("/Location");
            e.mDate = text("/M");
            if (v.hasKey("/SubFilter") && v.getKey("/SubFilter").isName())
                e.subFilter = QString::fromStdString(v.getKey("/SubFilter").getName());
            out->append(e);
        }
    }
    if (field.hasKey("/Kids")) {
        QPDFObjectHandle kids = field.getKey("/Kids");
        if (kids.isArray())
            for (int i = 0; i < kids.getArrayNItems(); ++i)
                collectSignatures(kids.getArrayItem(i), out, seen);
    }
}

QString parsePdfDate(const QString& m) {
    // D:YYYYMMDDHHmmss…
    if (m.size() < 16 || !m.startsWith(QLatin1String("D:")))
        return QString();
    const QDateTime dt =
        QDateTime::fromString(m.mid(2, 14), QStringLiteral("yyyyMMddHHmmss"));
    return dt.isValid() ? dt.toString(Qt::TextDate) : QString();
}

} // namespace

QStringList Signer::availableCertificates() {
    QStringList names;
    eachSigningCert([&](PCCERT_CONTEXT c) {
        const QString d = certDisplay(c);
        if (!names.contains(d))
            names << d;
        return false; // keep enumerating
    });
    return names;
}

bool Signer::sign(const QString& inputPath, const QString& outputPath, const QString& certName,
                  const QString& password, const QString& reason, const QString& location,
                  int page, const QRectF& rect, const QString& imagePath, QString* error) {
    Q_UNUSED(password); // Windows prompts for smartcard PINs itself.
    const auto fail = [&](const QString& m) {
        if (error)
            *error = m;
        return false;
    };

    PCCERT_CONTEXT cert =
        eachSigningCert([&](PCCERT_CONTEXT c) { return certDisplay(c) == certName; });
    if (!cert)
        return fail(QObject::tr("That signing certificate is no longer available."));
    const QString cn = certCommonName(cert);

    // 1) Read the structure we extend: the page carrying the signature widget
    //    and the AcroForm (created here when the document has none).
    QPDF pdf;
    int rootId = 0, rootGen = 0, originalSize = 0;
    QByteArray idArray;
    QList<Obj> objs;
    int nextId = 0;
    int sigId = 0;
    try {
        pdf.processFile(inputPath.toLocal8Bit().constData());
        QPDFObjectHandle root = pdf.getRoot();
        rootId = root.getObjGen().getObj();
        rootGen = root.getObjGen().getGen();

        QPDFObjectHandle trailer = pdf.getTrailer();
        if (trailer.hasKey("/Size"))
            originalSize = int(trailer.getKey("/Size").getIntValue());
        if (trailer.hasKey("/ID"))
            idArray = QByteArray::fromStdString(trailer.getKey("/ID").unparseResolved());

        std::vector<QPDFPageObjectHelper> pages = QPDFPageDocumentHelper(pdf).getAllPages();
        if (page < 0 || size_t(page) >= pages.size())
            return fail(QObject::tr("The page to sign no longer exists."));
        QPDFObjectHandle pageObj = pages[size_t(page)].getObjectHandle();
        if (!pageObj.isIndirect())
            return fail(QObject::tr("The page to sign couldn't be addressed."));

        nextId = std::max(originalSize, rootId + 1);
        sigId = nextId++;
        const int widgetId = nextId++;
        const int apId = nextId++;
        const int fontId = nextId++;
        int imgId = 0;

        // 2) The appearance: an image fitted into the box, or a text block.
        const double w = rect.width(), h = rect.height();
        QByteArray resources = "<< /Font << /Helv " + QByteArray::number(fontId) + " 0 R >>";
        QByteArray content = "q\n";
        QImage image;
        if (!imagePath.isEmpty())
            image.load(imagePath);
        if (!image.isNull()) {
            imgId = nextId++;
            // Composite over white (JPEG has no alpha) and fit, centred.
            QImage rgb(image.size(), QImage::Format_RGB888);
            rgb.fill(Qt::white);
            {
                QPainter p(&rgb);
                p.drawImage(0, 0, image);
            }
            QByteArray jpeg;
            {
                QBuffer buf(&jpeg);
                buf.open(QIODevice::WriteOnly);
                rgb.save(&buf, "JPEG", 90);
            }
            const double scale =
                std::min(w / std::max(1, rgb.width()), h / std::max(1, rgb.height()));
            const double dw = rgb.width() * scale, dh = rgb.height() * scale;
            const double tx = (w - dw) / 2.0, ty = (h - dh) / 2.0;
            content += num(dw) + " 0 0 " + num(dh) + ' ' + num(tx) + ' ' + num(ty)
                + " cm /Img Do\n";
            resources += " /XObject << /Img " + QByteArray::number(imgId) + " 0 R >>";
            objs << Obj{imgId, 0,
                        makeStreamObj(imgId, jpeg,
                                      "/Type /XObject /Subtype /Image /Width "
                                          + QByteArray::number(rgb.width()) + " /Height "
                                          + QByteArray::number(rgb.height())
                                          + " /ColorSpace /DeviceRGB /BitsPerComponent 8"
                                            " /Filter /DCTDecode")};
        } else {
            const QString dateLine =
                QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
            QStringList lines{QObject::tr("Digitally signed by"), cn};
            if (!reason.isEmpty())
                lines << reason;
            lines << dateLine;
            content += "BT /Helv 9 Tf 0 g 1 0 0 1 6 " + num(h - 14) + " Tm 11 TL\n";
            for (const QString& line : lines)
                content += '(' + contentText(line) + ") Tj T*\n";
            content += "ET\n";
        }
        content += "Q";
        resources += " >>";

        objs << Obj{fontId, 0,
                    makeObj(fontId, 0,
                            "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica"
                            " /Encoding /WinAnsiEncoding >>")};
        objs << Obj{apId, 0,
                    makeStreamObj(apId, content,
                                  "/Type /XObject /Subtype /Form /BBox [0 0 " + num(w) + ' '
                                      + num(h) + "] /Resources " + resources)};

        // 3) The signature value: fixed-width placeholders for /ByteRange and a
        //    generous /Contents slot, patched after the file is assembled.
        const QByteArray brSlots(32, ' ');
        const QByteArray contentsSlot(2 * 16384, '0');
        QByteArray sig = "<< /Type /Sig /Filter /Adobe.PPKLite /SubFilter /adbe.pkcs7.detached"
                         " /Name "
            + pdfTextString(cn) + " /M " + pdfDate(QDateTime::currentDateTime());
        if (!reason.isEmpty())
            sig += " /Reason " + pdfTextString(reason);
        if (!location.isEmpty())
            sig += " /Location " + pdfTextString(location);
        sig += " /ByteRange [0 " + brSlots + "] /Contents <" + contentsSlot + "> >>";
        objs << Obj{sigId, 0, makeObj(sigId, 0, sig)};

        // 4) The widget that shows the appearance and owns the value.
        const QByteArray fieldName =
            "(FeatherSig" + QByteArray::number(QDateTime::currentMSecsSinceEpoch()) + ')';
        QByteArray widget = "<< /Type /Annot /Subtype /Widget /FT /Sig /F 132 /Rect ["
            + num(rect.left()) + ' ' + num(rect.top()) + ' ' + num(rect.left() + rect.width())
            + ' ' + num(rect.top() + rect.height()) + "] /T " + fieldName + " /P "
            + QByteArray::number(pageObj.getObjGen().getObj()) + " 0 R /V "
            + QByteArray::number(sigId) + " 0 R /AP << /N " + QByteArray::number(apId)
            + " 0 R >> >>";
        objs << Obj{widgetId, 0, makeObj(widgetId, 0, widget)};

        // 5) Wire the widget into the AcroForm (creating one if needed) and the
        //    page's /Annots, mirroring LtvSigner's DocTimeStamp plumbing.
        QPDFObjectHandle acro = root.getKey("/AcroForm");
        const QByteArray drEntry =
            "<< /Font << /Helv " + QByteArray::number(fontId) + " 0 R >> >>";
        if (acro.isDictionary() && acro.isIndirect()) {
            QPDFObjectHandle fields = acro.getKey("/Fields");
            QMap<QByteArray, QByteArray> ov;
            ov["/SigFlags"] = "3";
            if (!acro.hasKey("/DR"))
                ov["/DR"] = drEntry;
            if (fields.isIndirect()) {
                objs << Obj{fields.getObjGen().getObj(), fields.getObjGen().getGen(),
                            makeObj(fields.getObjGen().getObj(), fields.getObjGen().getGen(),
                                    arrayWithAppended(fields, widgetId))};
            } else {
                ov["/Fields"] = arrayWithAppended(fields, widgetId);
            }
            objs << Obj{acro.getObjGen().getObj(), acro.getObjGen().getGen(),
                        makeObj(acro.getObjGen().getObj(), acro.getObjGen().getGen(),
                                rebuildDict(acro, ov))};
        } else {
            const int acroId = nextId++;
            objs << Obj{acroId, 0,
                        makeObj(acroId, 0,
                                "<< /Fields [ " + QByteArray::number(widgetId)
                                    + " 0 R ] /SigFlags 3 /DR " + drEntry + " >>")};
            QMap<QByteArray, QByteArray> ov;
            ov["/AcroForm"] = QByteArray::number(acroId) + " 0 R";
            objs << Obj{rootId, rootGen, makeObj(rootId, rootGen, rebuildDict(root, ov))};
        }

        QPDFObjectHandle annots = pageObj.getKey("/Annots");
        if (annots.isArray() && annots.isIndirect()) {
            objs << Obj{annots.getObjGen().getObj(), annots.getObjGen().getGen(),
                        makeObj(annots.getObjGen().getObj(), annots.getObjGen().getGen(),
                                arrayWithAppended(annots, widgetId))};
        } else {
            QMap<QByteArray, QByteArray> ov;
            ov["/Annots"] = arrayWithAppended(annots, widgetId);
            objs << Obj{pageObj.getObjGen().getObj(), pageObj.getObjGen().getGen(),
                        makeObj(pageObj.getObjGen().getObj(), pageObj.getObjGen().getGen(),
                                rebuildDict(pageObj, ov))};
        }
    } catch (const std::exception& e) {
        CertFreeCertificateContext(cert);
        return fail(QObject::tr("The document couldn't be read for signing. %1")
                        .arg(QString::fromUtf8(e.what())));
    }

    const QByteArray original = [&] {
        QFile f(inputPath);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }();
    if (original.isEmpty()) {
        CertFreeCertificateContext(cert);
        return fail(QObject::tr("The document couldn't be opened for signing."));
    }
    const qint64 prevXref = lastStartxref(original);
    if (prevXref < 0) {
        CertFreeCertificateContext(cert);
        return fail(QObject::tr("The document's cross-reference table couldn't be located."));
    }

    const int newSize = std::max(originalSize, nextId);
    QByteArray bytes =
        writeIncrementalUpdate(original, prevXref, objs, rootId, rootGen, newSize, idArray);

    // 6) Locate our placeholders (within the appended region only), fill the
    //    ByteRange, and sign everything except the contents gap.
    const int from = original.size();
    const QByteArray brTag = "/ByteRange [0 ";
    const int brAt = bytes.indexOf(brTag, from);
    const int contAt = bytes.indexOf("/Contents <", from);
    if (brAt < 0 || contAt < 0) {
        CertFreeCertificateContext(cert);
        return fail(QObject::tr("The signature placeholder couldn't be located."));
    }
    const int slot = brAt + brTag.size();
    const int gapStart = contAt + 10; // the '<' of /Contents <...>
    const int gapEnd = bytes.indexOf('>', gapStart);
    if (gapEnd < 0) {
        CertFreeCertificateContext(cert);
        return fail(QObject::tr("The signature placeholder couldn't be located."));
    }

    const qint64 a = gapStart;
    const qint64 b = gapEnd + 1;
    const qint64 c = bytes.size() - (gapEnd + 1);
    char br[40];
    std::snprintf(br, sizeof(br), "%10lld %10lld %10lld", static_cast<long long>(a),
                  static_cast<long long>(b), static_cast<long long>(c));
    std::memcpy(bytes.data() + slot, br, 32);

    const QByteArray cms = cmsSignDetached(cert, bytes, gapStart, gapEnd + 1, error);
    CertFreeCertificateContext(cert);
    if (cms.isEmpty())
        return false;
    const QByteArray cmsHex = cms.toHex();
    if (cmsHex.size() > gapEnd - (gapStart + 1))
        return fail(QObject::tr("The signature is larger than the reserved space."));
    std::memcpy(bytes.data() + gapStart + 1, cmsHex.constData(), cmsHex.size());

    QFile out(outputPath);
    if (!out.open(QIODevice::WriteOnly))
        return fail(QObject::tr("The signed file couldn't be written."));
    if (out.write(bytes) != bytes.size())
        return fail(QObject::tr("The signed file couldn't be fully written."));
    return true;
}

bool Signer::timestamp(const QString& filePath, const QString& tsaUrl,
                       const QString& outTokenPath, QString* error) {
    const auto fail = [&](const QString& m) {
        if (error)
            *error = m;
        return false;
    };
    const QString openssl = ToolLocator::openssl();
    const QString curl = QStandardPaths::findExecutable(QStringLiteral("curl"));
    if (openssl.isEmpty() || curl.isEmpty())
        return fail(QObject::tr("Trusted timestamping needs 'openssl' and 'curl' on your PATH."));
    if (tsaUrl.isEmpty())
        return fail(QObject::tr("No timestamp authority (TSA) URL was given."));
    if (!QFileInfo::exists(filePath))
        return fail(QObject::tr("The file to timestamp no longer exists."));

    QTemporaryDir tmp;
    if (!tmp.isValid())
        return fail(QObject::tr("Couldn't create a temporary working directory."));
    const QString tsq = tmp.filePath(QStringLiteral("request.tsq"));
    const QString tsr = tmp.filePath(QStringLiteral("response.tsr"));

    const auto okRun = [](const QString& prog, const QStringList& args, int ms) {
        QProcess p;
        p.start(prog, args);
        return p.waitForFinished(ms) && p.exitStatus() == QProcess::NormalExit
            && p.exitCode() == 0;
    };

    // 1) Build an RFC 3161 query over the file's SHA-256 (request the TSA's cert).
    if (!okRun(openssl,
               {QStringLiteral("ts"), QStringLiteral("-query"), QStringLiteral("-data"), filePath,
                QStringLiteral("-sha256"), QStringLiteral("-cert"), QStringLiteral("-out"), tsq},
               15000))
        return fail(QObject::tr("Couldn't build the timestamp request."));

    // 2) POST it to the timestamp authority.
    if (!okRun(curl,
               {QStringLiteral("-s"), QStringLiteral("-S"), QStringLiteral("--max-time"),
                QStringLiteral("25"), QStringLiteral("-H"),
                QStringLiteral("Content-Type: application/timestamp-query"),
                QStringLiteral("--data-binary"), QStringLiteral("@") + tsq, tsaUrl,
                QStringLiteral("-o"), tsr},
               30000))
        return fail(QObject::tr("Couldn't reach the timestamp authority. Check the TSA URL and "
                                "your connection."));

    // 3) Confirm the reply is a well-formed RFC 3161 timestamp response.
    if (!okRun(openssl,
               {QStringLiteral("ts"), QStringLiteral("-reply"), QStringLiteral("-in"), tsr,
                QStringLiteral("-text")},
               15000))
        return fail(QObject::tr("The timestamp authority's reply wasn't a valid RFC 3161 token."));

    // 4) Save the token next to the signed file.
    QFile::remove(outTokenPath);
    if (!QFile::copy(tsr, outTokenPath))
        return fail(QObject::tr("Couldn't save the timestamp token."));
    return true;
}

QList<Signer::SignatureInfo> Signer::verify(const QString& path) {
    QList<SignatureInfo> out;

    const QByteArray file = [&] {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }();
    if (file.isEmpty())
        return out;

    QList<SigEntry> entries;
    try {
        QPDF pdf;
        pdf.processFile(path.toLocal8Bit().constData());
        QPDFObjectHandle acro = pdf.getRoot().getKey("/AcroForm");
        if (acro.isDictionary() && acro.hasKey("/Fields")) {
            QPDFObjectHandle fields = acro.getKey("/Fields");
            std::set<QPDFObjGen> seen;
            if (fields.isArray())
                for (int i = 0; i < fields.getArrayNItems(); ++i)
                    collectSignatures(fields.getArrayItem(i), &entries, &seen);
        }
    } catch (const std::exception&) {
        return out;
    }

    for (const SigEntry& e : entries) {
        // Document timestamps aren't user signatures; the Signatures dialog
        // lists people, not TSAs.
        if (e.subFilter == QLatin1String("/ETSI.RFC3161"))
            continue;

        SignatureInfo s;
        s.signer = e.name;
        s.reason = e.reason;
        s.location = e.location;
        s.time = parsePdfDate(e.mDate);

        // Assemble the signed segments from the declared byte ranges.
        bool sane = !e.ranges.isEmpty() && e.ranges.size() % 2 == 0;
        std::vector<const BYTE*> parts;
        std::vector<DWORD> sizes;
        for (int i = 0; sane && i + 1 < e.ranges.size(); i += 2) {
            const qint64 off = e.ranges[i], len = e.ranges[i + 1];
            if (off < 0 || len < 0 || off + len > file.size()) {
                sane = false;
                break;
            }
            parts.push_back(reinterpret_cast<const BYTE*>(file.constData()) + off);
            sizes.push_back(DWORD(len));
        }
        const QByteArray cms = e.contents.left(int(derTotalLen(e.contents)));
        if (!sane || cms.isEmpty()) {
            s.status = QObject::tr("The signature is malformed.");
            out.append(s);
            continue;
        }

        CRYPT_VERIFY_MESSAGE_PARA vp{};
        vp.cbSize = sizeof(vp);
        vp.dwMsgAndCertEncodingType = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;
        PCCERT_CONTEXT signerCert = nullptr;
        const BOOL digestOk = CryptVerifyDetachedMessageSignature(
            &vp, 0, reinterpret_cast<const BYTE*>(cms.constData()), DWORD(cms.size()),
            DWORD(parts.size()), parts.data(), sizes.data(), &signerCert);

        if (!digestOk) {
            s.status = QObject::tr("The document was changed after it was signed.");
            out.append(s);
            continue;
        }
        s.intact = true;

        if (signerCert && s.signer.isEmpty())
            s.signer = certCommonName(signerCert);

        // Chain the signer to a root this machine trusts, resolving
        // intermediates from the certificates carried inside the CMS.
        bool trusted = false;
        if (signerCert) {
            CRYPT_DATA_BLOB blob;
            blob.pbData =
                reinterpret_cast<BYTE*>(const_cast<char*>(cms.constData()));
            blob.cbData = DWORD(cms.size());
            HCERTSTORE msgStore =
                CertOpenStore(CERT_STORE_PROV_PKCS7, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0,
                              0, &blob);
            CERT_CHAIN_PARA chainPara{};
            chainPara.cbSize = sizeof(chainPara);
            PCCERT_CHAIN_CONTEXT chain = nullptr;
            if (CertGetCertificateChain(nullptr, signerCert, nullptr, msgStore, &chainPara, 0,
                                        nullptr, &chain)
                && chain) {
                trusted = chain->TrustStatus.dwErrorStatus == CERT_TRUST_NO_ERROR;
                CertFreeCertificateChain(chain);
            }
            if (msgStore)
                CertCloseStore(msgStore, 0);
        }

        s.valid = trusted;
        s.status = trusted
                       ? QObject::tr("Valid - the document is intact.")
                       : QObject::tr("The document is intact, but the certificate isn't trusted "
                                     "on this machine.");
        if (signerCert)
            CertFreeCertificateContext(signerCert);
        out.append(s);
    }
    return out;
}
