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

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <memory>

#include <poppler-converter.h>
#include <poppler-form.h>
#include <poppler-qt6.h>

namespace {
QString statusText(Poppler::SignatureValidationInfo::SignatureStatus s, bool* valid) {
    *valid = false;
    switch (s) {
    case Poppler::SignatureValidationInfo::SignatureValid:
        *valid = true;
        return QObject::tr("Valid - the document is intact.");
    case Poppler::SignatureValidationInfo::SignatureDigestMismatch:
        return QObject::tr("The document was changed after it was signed.");
    case Poppler::SignatureValidationInfo::SignatureInvalid:
        return QObject::tr("Invalid signature.");
    case Poppler::SignatureValidationInfo::SignatureDecodingError:
        return QObject::tr("The signature is malformed.");
    case Poppler::SignatureValidationInfo::SignatureNotFound:
        return QObject::tr("No signature found.");
    default:
        return QObject::tr("The signature could not be verified.");
    }
}

// The NSS database the signing stack uses. Empty means the shared user store.
QString g_nssDir;
QString currentNssDir() {
    return g_nssDir.isEmpty() ? QDir::homePath() + QStringLiteral("/.pki/nssdb") : g_nssDir;
}
QString nssDbArg() { return QStringLiteral("sql:") + currentNssDir(); }

// Run a tool, capturing stdout. True on a clean exit 0. We answer a possible
// confirmation prompt with a newline (modutil still asks "press enter to
// continue" even with -force when p11-kit is enabled) and close stdin, so the
// tool proceeds instead of hanging on input.
bool runTool(const QString& prog, const QStringList& args, QString* stdoutText, int ms = 20000) {
    QProcess p;
    p.start(prog, args);
    if (!p.waitForStarted(ms))
        return false;
    p.write("\n");
    p.closeWriteChannel();
    if (!p.waitForFinished(ms)) {
        p.kill();
        p.waitForFinished(2000);
        return false;
    }
    if (stdoutText)
        *stdoutText = QString::fromUtf8(p.readAllStandardOutput());
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

// Create the NSS database (empty password) if it doesn't exist yet.
bool ensureNssDb(QString* error) {
    QDir().mkpath(currentNssDir());
    if (QFileInfo::exists(currentNssDir() + QStringLiteral("/cert9.db")))
        return true;
    const QString certutil = QStandardPaths::findExecutable(QStringLiteral("certutil"));
    if (certutil.isEmpty()) {
        if (error)
            *error = QObject::tr("'certutil' (nss-tools) is needed to set up the certificate store.");
        return false;
    }
    if (!runTool(certutil,
                 {QStringLiteral("-N"), QStringLiteral("-d"), nssDbArg(),
                  QStringLiteral("--empty-password")},
                 nullptr)) {
        if (error)
            *error = QObject::tr("Couldn't create the certificate store.");
        return false;
    }
    return true;
}
} // namespace

void Signer::useNssDatabase(const QString& dir) {
    g_nssDir = dir;
    Poppler::setNSSDir(dir);
}

bool Signer::hasSecurityDeviceTools() {
    return !QStandardPaths::findExecutable(QStringLiteral("modutil")).isEmpty();
}

bool Signer::addSecurityDevice(const QString& name, const QString& modulePath, QString* error) {
    const auto fail = [&](const QString& m) {
        if (error)
            *error = m;
        return false;
    };
    const QString modutil = QStandardPaths::findExecutable(QStringLiteral("modutil"));
    if (modutil.isEmpty())
        return fail(QObject::tr("'modutil' (nss-tools) isn't installed."));
    if (name.trimmed().isEmpty())
        return fail(QObject::tr("Give the security device a name."));
    if (!QFileInfo::exists(modulePath))
        return fail(QObject::tr("The PKCS#11 module '%1' wasn't found.").arg(modulePath));
    if (!ensureNssDb(error))
        return false;
    QString outText;
    if (!runTool(modutil,
                 {QStringLiteral("-force"), QStringLiteral("-add"), name, QStringLiteral("-libfile"),
                  modulePath, QStringLiteral("-dbdir"), nssDbArg()},
                 &outText))
        return fail(QObject::tr("Couldn't register the security device. %1").arg(outText.trimmed()));
    return true;
}

bool Signer::removeSecurityDevice(const QString& name, QString* error) {
    const QString modutil = QStandardPaths::findExecutable(QStringLiteral("modutil"));
    if (modutil.isEmpty()) {
        if (error)
            *error = QObject::tr("'modutil' (nss-tools) isn't installed.");
        return false;
    }
    QString outText;
    if (!runTool(modutil,
                 {QStringLiteral("-force"), QStringLiteral("-delete"), name, QStringLiteral("-dbdir"),
                  nssDbArg()},
                 &outText)) {
        if (error)
            *error = QObject::tr("Couldn't remove the security device. %1").arg(outText.trimmed());
        return false;
    }
    return true;
}

QStringList Signer::securityDevices() {
    QStringList names;
    const QString modutil = QStandardPaths::findExecutable(QStringLiteral("modutil"));
    if (modutil.isEmpty())
        return names;
    QString out;
    if (!runTool(modutil, {QStringLiteral("-list"), QStringLiteral("-dbdir"), nssDbArg()}, &out))
        return names;
    // Numbered entries like "  2. MyToken"; skip the built-in internal module.
    static const QRegularExpression re(QStringLiteral("^\\s*\\d+\\.\\s+(.+?)\\s*$"));
    for (const QString& line : out.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch m = re.match(line);
        if (m.hasMatch() && !m.captured(1).contains(QStringLiteral("NSS Internal")))
            names << m.captured(1);
    }
    return names;
}

QStringList Signer::availableCertificates() {
    QStringList names;
    for (const Poppler::CertificateInfo& c : Poppler::getAvailableSigningCertificates()) {
        const QString cn = c.subjectInfo(Poppler::CertificateInfo::CommonName);
        names << (cn.isEmpty() ? c.nickName() : cn);
    }
    return names;
}

bool Signer::sign(const QString& inputPath, const QString& outputPath, const QString& certName,
                  const QString& password, const QString& reason, const QString& location,
                  int page, const QRectF& rect, const QString& imagePath, QString* error) {
    // Resolve the display name back to the NSS nickname the backend signs with.
    QString nickname;
    for (const Poppler::CertificateInfo& c : Poppler::getAvailableSigningCertificates()) {
        const QString cn = c.subjectInfo(Poppler::CertificateInfo::CommonName);
        if (certName == cn || certName == c.nickName()) {
            nickname = c.nickName();
            break;
        }
    }
    if (nickname.isEmpty()) {
        if (error)
            *error = QStringLiteral("That signing certificate is no longer available.");
        return false;
    }

    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(inputPath);
    if (!doc || doc->isLocked()) {
        if (error)
            *error = QStringLiteral("The document couldn't be opened for signing.");
        return false;
    }

    const QString target = outputPath + QStringLiteral(".feather-tmp");
    std::unique_ptr<Poppler::PDFConverter> conv = doc->pdfConverter();
    conv->setOutputFileName(target);

    Poppler::PDFConverter::NewSignatureData data;
    data.setCertNickname(nickname);
    data.setPassword(password);
    data.setPage(page);
    data.setBoundingRectangle(rect);
    QString text = certName;
    if (!reason.isEmpty())
        text += QStringLiteral("\n") + reason;
    data.setSignatureText(QObject::tr("Digitally signed by\n%1").arg(text));
    // A graphical signature: draw the chosen image behind the appearance widget.
    if (!imagePath.isEmpty() && QFileInfo::exists(imagePath))
        data.setImagePath(imagePath);
    if (!reason.isEmpty())
        data.setReason(reason);
    if (!location.isEmpty())
        data.setLocation(location);
    // A unique field name per signature so multiple signatures can coexist.
    data.setFieldPartialName(
        QStringLiteral("FeatherSig%1").arg(QDateTime::currentMSecsSinceEpoch()));

    if (!conv->sign(data)) {
        if (error)
            *error = QStringLiteral("Signing failed. Check the certificate password.");
        QFile::remove(target);
        return false;
    }

    QFile::remove(outputPath);
    if (!QFile::rename(target, outputPath)) {
        if (error)
            *error = QStringLiteral("The signed file couldn't replace the original.");
        QFile::remove(target);
        return false;
    }
    return true;
}

bool Signer::timestamp(const QString& filePath, const QString& tsaUrl,
                       const QString& outTokenPath, QString* error) {
    const auto fail = [&](const QString& m) {
        if (error)
            *error = m;
        return false;
    };
    const QString openssl = QStandardPaths::findExecutable(QStringLiteral("openssl"));
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
    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(path);
    if (!doc || doc->isLocked())
        return out;

    for (const auto& sig : doc->signatures()) {
        const Poppler::SignatureValidationInfo info =
            sig->validate(Poppler::FormFieldSignature::ValidateVerifyCertificate);
        SignatureInfo s;
        s.signer = info.signerName();
        s.status = statusText(info.signatureStatus(), &s.valid);
        const time_t t = info.signingTime();
        if (t > 0)
            s.time = QDateTime::fromSecsSinceEpoch(t).toString(Qt::TextDate);
        s.reason = info.reason();
        s.location = info.location();
        out.append(s);
    }
    return out;
}
