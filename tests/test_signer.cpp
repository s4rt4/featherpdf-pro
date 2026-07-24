// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Exercises the CNG signing backend end to end, fully offline: a throwaway
// self-signed certificate (CNG key + CertCreateSelfSignCertificate) is placed
// in the user's "MY" store for the duration of the run, documents are signed
// and verified with CryptoAPI, tampering is detected, LTV embeds the
// certificate, and a loopback RFC 3161 TSA proves the archive-timestamp path.
// A self-signed certificate can't chain to a trusted root, so the assertions
// check SignatureInfo::intact (the CMS verifies over the byte ranges), not
// `valid`. Everything the setup creates is removed in cleanupTestCase().

#include "backends/LtvSigner.h"
#include "backends/Signer.h"
#include "backends/ToolLocator.h"

#include <QDir>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QImage>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QProcess>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

#include <atomic>
#include <thread>
#include <vector>

#define NOMINMAX
#include <windows.h>

#include <ncrypt.h>
#include <wincrypt.h>

#include <memory>

#include <qpdf/Buffer.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>

namespace {
bool run(const QString& prog, const QStringList& args, int ms = 30000) {
    QProcess p;
    p.start(prog, args);
    return p.waitForFinished(ms) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}
} // namespace

class TestSigner : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_pdf;
    QString m_png;
    QString m_keyName;      // CNG key container backing the test certificate
    QString m_certDisplay;  // the entry availableCertificates() shows for it
    PCCERT_CONTEXT m_storeCert = nullptr; // the copy living in the MY store
    HCERTSTORE m_store = nullptr;         // kept open so deletion sticks

    static constexpr const wchar_t* kSubject = L"CN=Feather Test Signer";

    // Remove every test certificate (ours and any a crashed run left behind),
    // deleting each one's key container when it still exists. A stale
    // same-name certificate whose key is gone would otherwise shadow the
    // fresh one and break signing with NTE_BAD_KEYSET.
    static void sweepTestCertificates() {
        HCERTSTORE store =
            CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"MY");
        if (!store)
            return;
        for (;;) {
            PCCERT_CONTEXT c = CertFindCertificateInStore(
                store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR,
                L"Feather Test Signer", nullptr);
            if (!c)
                break;
            DWORD cb = 0;
            if (CertGetCertificateContextProperty(c, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &cb)) {
                std::vector<BYTE> buf(cb);
                if (CertGetCertificateContextProperty(c, CERT_KEY_PROV_INFO_PROP_ID, buf.data(),
                                                      &cb)) {
                    auto* kpi = reinterpret_cast<CRYPT_KEY_PROV_INFO*>(buf.data());
                    NCRYPT_PROV_HANDLE prov = 0;
                    if (NCryptOpenStorageProvider(&prov, kpi->pwszProvName, 0) == ERROR_SUCCESS) {
                        NCRYPT_KEY_HANDLE key = 0;
                        if (NCryptOpenKey(prov, &key, kpi->pwszContainerName, kpi->dwKeySpec, 0)
                            == ERROR_SUCCESS)
                            NCryptDeleteKey(key, 0);
                        NCryptFreeObject(prov);
                    }
                }
            }
            CertDeleteCertificateFromStore(c); // frees the context
        }
        CertCloseStore(store, 0);
    }

    bool addTestCertificate() {
        m_keyName = QStringLiteral("FeatherTestKey-%1")
                        .arg(QCoreApplication::applicationPid());

        NCRYPT_PROV_HANDLE prov = 0;
        if (NCryptOpenStorageProvider(&prov, MS_KEY_STORAGE_PROVIDER, 0) != ERROR_SUCCESS)
            return false;
        NCRYPT_KEY_HANDLE key = 0;
        const auto keyName = reinterpret_cast<const wchar_t*>(m_keyName.utf16());
        if (NCryptCreatePersistedKey(prov, &key, NCRYPT_RSA_ALGORITHM, keyName, 0, 0)
            != ERROR_SUCCESS) {
            NCryptFreeObject(prov);
            return false;
        }
        DWORD bits = 2048;
        NCryptSetProperty(key, NCRYPT_LENGTH_PROPERTY, reinterpret_cast<PBYTE>(&bits),
                          sizeof(bits), 0);
        if (NCryptFinalizeKey(key, 0) != ERROR_SUCCESS) {
            NCryptFreeObject(key);
            NCryptFreeObject(prov);
            return false;
        }

        BYTE nameBuf[256];
        DWORD nameLen = sizeof(nameBuf);
        if (!CertStrToNameW(X509_ASN_ENCODING, kSubject, CERT_X500_NAME_STR, nullptr, nameBuf,
                            &nameLen, nullptr)) {
            NCryptFreeObject(key);
            NCryptFreeObject(prov);
            return false;
        }
        CERT_NAME_BLOB subject{nameLen, nameBuf};

        // Link the certificate to the persisted key, so a context enumerated
        // from the store can re-open the key to sign.
        CRYPT_KEY_PROV_INFO kpi{};
        kpi.pwszContainerName = const_cast<LPWSTR>(keyName);
        kpi.pwszProvName = const_cast<LPWSTR>(MS_KEY_STORAGE_PROVIDER);

        CRYPT_ALGORITHM_IDENTIFIER alg{};
        alg.pszObjId = const_cast<char*>(szOID_RSA_SHA256RSA);

        SYSTEMTIME end;
        GetSystemTime(&end);
        end.wYear += 1;
        PCCERT_CONTEXT cert =
            CertCreateSelfSignCertificate(key, &subject, 0, &kpi, &alg, nullptr, &end, nullptr);
        NCryptFreeObject(key);
        NCryptFreeObject(prov);
        if (!cert)
            return false;

        m_store =
            CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"MY");
        if (!m_store) {
            CertFreeCertificateContext(cert);
            return false;
        }
        const BOOL added = CertAddCertificateContextToStore(
            m_store, cert, CERT_STORE_ADD_REPLACE_EXISTING, &m_storeCert);
        CertFreeCertificateContext(cert);
        return added == TRUE;
    }

    void removeTestCertificate() {
        if (m_storeCert) {
            CertDeleteCertificateFromStore(m_storeCert); // also frees the context
            m_storeCert = nullptr;
        }
        if (m_store) {
            CertCloseStore(m_store, 0);
            m_store = nullptr;
        }
        if (!m_keyName.isEmpty()) {
            NCRYPT_PROV_HANDLE prov = 0;
            if (NCryptOpenStorageProvider(&prov, MS_KEY_STORAGE_PROVIDER, 0) == ERROR_SUCCESS) {
                NCRYPT_KEY_HANDLE key = 0;
                if (NCryptOpenKey(prov, &key,
                                  reinterpret_cast<const wchar_t*>(m_keyName.utf16()), 0, 0)
                    == ERROR_SUCCESS)
                    NCryptDeleteKey(key, 0); // frees the handle
                NCryptFreeObject(prov);
            }
        }
        sweepTestCertificates(); // belt and braces: leave the store spotless
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        const QString d = m_dir.path();

        // A one-page PDF to sign, and a PNG to use as the graphical appearance.
        m_pdf = d + QStringLiteral("/in.pdf");
        {
            QPdfWriter w(m_pdf);
            w.setPageSize(QPageSize(QPageSize::A4));
            QPainter pa(&w);
            pa.drawText(120, 120, QStringLiteral("Feather signing test"));
            pa.end();
        }
        QVERIFY(QFileInfo::exists(m_pdf));

        m_png = d + QStringLiteral("/sig.png");
        {
            QImage img(220, 90, QImage::Format_ARGB32);
            img.fill(Qt::white);
            QPainter pa(&img);
            pa.setPen(Qt::darkBlue);
            pa.drawText(12, 50, QStringLiteral("V. Signature"));
            pa.end();
            QVERIFY(img.save(m_png));
        }

        // The throwaway certificate the signing tests use. If the environment
        // forbids writing to the user store, those tests skip rather than fail.
        sweepTestCertificates(); // a crashed run must not shadow the fresh cert
        if (!addTestCertificate())
            return;
        for (const QString& c : Signer::availableCertificates())
            if (c.startsWith(QLatin1String("Feather Test Signer ("))) {
                m_certDisplay = c;
                break;
            }
    }

    void cleanupTestCase() { removeTestCertificate(); }

    // The store certificate surfaces in the list with its expiry.
    void certificateAppearsInList() {
        if (!m_storeCert)
            QSKIP("couldn't create a certificate in the user store");
        QVERIFY2(!m_certDisplay.isEmpty(),
                 qPrintable(Signer::availableCertificates().join(QStringLiteral(", "))));
    }

    // A graphical signature signs cleanly and verifies as intact.
    void signWithImageProducesIntactSignature() {
        if (m_certDisplay.isEmpty())
            QSKIP("signing certificate unavailable in this environment");

        const QString out = m_dir.path() + QStringLiteral("/signed.pdf");
        QString error;
        const bool ok = Signer::sign(m_pdf, out, m_certDisplay, QString(),
                                     QStringLiteral("Approved"), QStringLiteral("Jakarta"), 0,
                                     QRectF(48, 40, 220, 90), m_png, &error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(QFileInfo::exists(out));

        const QList<Signer::SignatureInfo> sigs = Signer::verify(out);
        QCOMPARE(sigs.size(), 1);
        QVERIFY2(sigs.first().intact, qPrintable(sigs.first().status));
        QCOMPARE(sigs.first().signer, QStringLiteral("Feather Test Signer"));
        QCOMPARE(sigs.first().reason, QStringLiteral("Approved"));
        QCOMPARE(sigs.first().location, QStringLiteral("Jakarta"));
        // Self-signed → intact but not chained to a trusted root.
        QVERIFY(!sigs.first().valid);
    }

    // Signing without an image still works (the text appearance path).
    void signWithoutImageStillWorks() {
        if (m_certDisplay.isEmpty())
            QSKIP("signing certificate unavailable in this environment");

        const QString out = m_dir.path() + QStringLiteral("/signed-text.pdf");
        QString error;
        const bool ok = Signer::sign(m_pdf, out, m_certDisplay, QString(), QString(), QString(),
                                     0, QRectF(48, 40, 220, 60), QString(), &error);
        QVERIFY2(ok, qPrintable(error));
        const QList<Signer::SignatureInfo> sigs = Signer::verify(out);
        QCOMPARE(sigs.size(), 1);
        QVERIFY2(sigs.first().intact, qPrintable(sigs.first().status));
    }

    // Any change to the signed bytes must break the signature.
    void tamperingIsDetected() {
        if (m_certDisplay.isEmpty())
            QSKIP("signing certificate unavailable in this environment");

        const QString out = m_dir.path() + QStringLiteral("/tampered.pdf");
        QString error;
        QVERIFY2(Signer::sign(m_pdf, out, m_certDisplay, QString(), QString(), QString(), 0,
                              QRectF(48, 40, 200, 56), QString(), &error),
                 qPrintable(error));

        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadWrite));
        f.seek(64); // well inside the first signed range
        char b = 0;
        f.peek(&b, 1);
        b = b == 'x' ? 'y' : 'x';
        f.write(&b, 1);
        f.close();

        const QList<Signer::SignatureInfo> sigs = Signer::verify(out);
        QCOMPARE(sigs.size(), 1);
        QVERIFY(!sigs.first().intact);
        QVERIFY(!sigs.first().valid);
    }

    // The timestamp helper refuses an empty TSA URL up front.
    void timestampRejectsEmptyUrl() {
        QString error;
        const QString token = m_dir.path() + QStringLiteral("/a.tsr");
        QVERIFY(!Signer::timestamp(m_pdf, QString(), token, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFileInfo::exists(token));
    }

    // And fails gracefully (no crash, clear message, no half-written token) when
    // the TSA can't be reached. 127.0.0.1:1 refuses the connection immediately.
    void timestampFailsWhenTsaUnreachable() {
        QString error;
        const QString token = m_dir.path() + QStringLiteral("/b.tsr");
        const bool ok =
            Signer::timestamp(m_pdf, QStringLiteral("http://127.0.0.1:1/"), token, &error);
        QVERIFY(!ok);
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFileInfo::exists(token));
    }

    // Long-term validation: adding a DSS to a signed document embeds the signer's
    // certificate, keeps the original signature intact (the incremental update
    // never touches the signed byte range), and records a VRI entry.
    void ltvAddsDssAndPreservesSignature() {
        if (m_certDisplay.isEmpty())
            QSKIP("signing certificate unavailable in this environment");
        if (ToolLocatorHasNoOpenssl())
            QSKIP("openssl unavailable (LtvSigner shells out to it)");

        const QString signed_ = m_dir.path() + QStringLiteral("/ltv-in.pdf");
        QString error;
        QVERIFY2(Signer::sign(m_pdf, signed_, m_certDisplay, QString(),
                              QStringLiteral("Approved"), QStringLiteral("Jakarta"), 0,
                              QRectF(48, 40, 220, 60), QString(), &error),
                 qPrintable(error));

        const QString out = m_dir.path() + QStringLiteral("/ltv-out.pdf");
        LtvSigner::Result res;
        QVERIFY2(LtvSigner::addValidationInfo(signed_, out, &error, &res), qPrintable(error));
        QVERIFY(QFileInfo::exists(out));

        // The store carried at least the signer's own certificate.
        QCOMPARE(res.signatures, 1);
        QVERIFY(res.certs >= 1);

        // The original signature still verifies: the byte range wasn't disturbed.
        const QList<Signer::SignatureInfo> sigs = Signer::verify(out);
        QCOMPARE(sigs.size(), 1);
        QVERIFY2(sigs.first().intact, qPrintable(sigs.first().status));

        // The catalog now references a /DSS with a non-empty /Certs and a /VRI.
        QPDF pdf;
        pdf.processFile(out.toLocal8Bit().constData());
        QPDFObjectHandle dss = pdf.getRoot().getKey("/DSS");
        QVERIFY(dss.isDictionary());
        QPDFObjectHandle certs = dss.getKey("/Certs");
        QVERIFY(certs.isArray());
        QVERIFY(certs.getArrayNItems() >= 1);
        QVERIFY(dss.getKey("/VRI").isDictionary());

        // The embedded cert really is a certificate stream (DER SEQUENCE).
        QPDFObjectHandle cert0 = certs.getArrayItem(0);
        std::shared_ptr<Buffer> buf = cert0.getStreamData();
        QVERIFY(buf && buf->getSize() > 0);
        QCOMPARE(static_cast<unsigned char>(buf->getBuffer()[0]),
                 static_cast<unsigned char>(0x30));
    }

    // LTV refuses a document that has no signatures, with a clear message.
    void ltvRejectsUnsignedDocument() {
        const QString out = m_dir.path() + QStringLiteral("/ltv-unsigned.pdf");
        QString error;
        QVERIFY(!LtvSigner::addValidationInfo(m_pdf, out, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFileInfo::exists(out));
    }

    // PAdES-LTA: embed an archive document timestamp via a loopback RFC 3161 TSA
    // (an openssl-backed responder on a local socket), fully offline. The
    // timestamp must cover the whole document and leave the signature intact.
    void docTimestampCoversWholeDocument() {
        if (m_certDisplay.isEmpty())
            QSKIP("signing certificate unavailable in this environment");
        // ToolLocator probes candidates and skips broken PATH copies (e.g. the
        // openssl.exe some Apache bundles ship).
        const QString openssl = ToolLocator::openssl();
        const QString curl = QStandardPaths::findExecutable(QStringLiteral("curl"));
        if (openssl.isEmpty() || curl.isEmpty())
            QSKIP("openssl/curl unavailable");

        // A TSA signing certificate (critical timeStamping EKU) + its config.
        const QString tsaDir = m_dir.path() + QStringLiteral("/tsa");
        QDir().mkpath(tsaDir);
        const QString crt = tsaDir + QStringLiteral("/tsa.crt");
        const QString key = tsaDir + QStringLiteral("/tsa.key");
        const QString cnf = tsaDir + QStringLiteral("/tsa.cnf");
        if (!run(openssl, {"req", "-x509", "-newkey", "rsa:2048", "-keyout", key, "-out", crt,
                           "-days", "2", "-nodes", "-subj", "/CN=Feather Local TSA", "-addext",
                           "extendedKeyUsage=critical,timeStamping"}))
            QSKIP("couldn't build a timeStamping certificate");
        {
            QFile f(cnf);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("[ tc ]\nserial = " + tsaDir.toUtf8() + "/serial\nsigner_cert = " + crt.toUtf8()
                    + "\nsigner_key = " + key.toUtf8()
                    + "\nsigner_digest = sha256\ndefault_policy = 1.2.3.4.1\ndigests = sha256\n"
                      "accuracy = secs:1\nclock_precision_digits = 0\nordering = yes\n"
                      "tsa_name = no\ness_cert_id_chain = no\n");
        }
        {
            QFile s(tsaDir + QStringLiteral("/serial"));
            QVERIFY(s.open(QIODevice::WriteOnly));
            s.write("01\n");
        }

        // The loopback TSA: answer each HTTP POST by signing the posted query.
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        std::atomic<int> served{0};
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            QTcpSocket* sock = server.nextPendingConnection();
            QByteArray req;
            int contentLength = -1;
            while (sock->state() == QAbstractSocket::ConnectedState) {
                if (!sock->waitForReadyRead(5000))
                    break;
                req += sock->readAll();
                const int h = req.indexOf("\r\n\r\n");
                if (h >= 0 && contentLength < 0) {
                    const QByteArray head = req.left(h).toLower();
                    const int cl = head.indexOf("content-length:");
                    if (cl >= 0)
                        contentLength =
                            head.mid(cl + 15, head.indexOf('\r', cl) - (cl + 15)).trimmed().toInt();
                }
                if (h >= 0 && contentLength >= 0 && req.size() - (h + 4) >= contentLength)
                    break;
            }
            const int h = req.indexOf("\r\n\r\n");
            const QByteArray body = h >= 0 ? req.mid(h + 4) : QByteArray();
            const QString tsq = tsaDir + QStringLiteral("/in.tsq");
            const QString tsr = tsaDir + QStringLiteral("/out.tsr");
            {
                QFile q(tsq);
                if (q.open(QIODevice::WriteOnly))
                    q.write(body);
            }
            run(openssl, {"ts", "-reply", "-queryfile", tsq, "-config", cnf, "-section", "tc",
                          "-out", tsr});
            QByteArray reply;
            {
                QFile r(tsr);
                if (r.open(QIODevice::ReadOnly))
                    reply = r.readAll();
            }
            sock->write("HTTP/1.1 200 OK\r\nContent-Type: application/timestamp-reply\r\n"
                        "Content-Length: "
                        + QByteArray::number(reply.size()) + "\r\nConnection: close\r\n\r\n" + reply);
            sock->flush();
            sock->waitForBytesWritten(5000);
            sock->disconnectFromHost();
            ++served;
        });
        const QString url =
            QStringLiteral("http://127.0.0.1:%1/").arg(server.serverPort());

        // Sign, then run the timestamp in a worker thread while this thread keeps
        // the event loop turning so the TSA socket gets serviced.
        const QString signed_ = m_dir.path() + QStringLiteral("/ts-in.pdf");
        QString error;
        QVERIFY2(Signer::sign(m_pdf, signed_, m_certDisplay, QString(), QString(), QString(), 0,
                              QRectF(48, 40, 200, 56), QString(), &error),
                 qPrintable(error));

        const QString out = m_dir.path() + QStringLiteral("/ts-out.pdf");
        std::atomic<bool> done{false};
        bool ok = false;
        QString tsErr;
        std::thread worker(
            [&] { ok = LtvSigner::addDocumentTimestamp(signed_, out, url, &tsErr); done = true; });
        QElapsedTimer clock;
        clock.start();
        while (!done && clock.elapsed() < 30000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        worker.join();

        QVERIFY2(ok, qPrintable(tsErr));
        QVERIFY(served > 0);
        QVERIFY(QFileInfo::exists(out));

        // The original signature is still intact (its byte range never moved).
        const QList<Signer::SignatureInfo> sigs = Signer::verify(out);
        bool originalIntact = false;
        for (const Signer::SignatureInfo& s : sigs)
            if (s.signer == QStringLiteral("Feather Test Signer") && s.intact)
                originalIntact = true;
        QVERIFY(originalIntact);

        // A /DocTimeStamp whose byte range runs to the end of the file (the whole
        // document is covered) and whose /Contents holds a non-empty token.
        QPDF pdf;
        pdf.processFile(out.toLocal8Bit().constData());
        const qint64 fileSize = QFileInfo(out).size();
        bool foundDts = false;
        QPDFObjectHandle fields =
            pdf.getRoot().getKey("/AcroForm").getKey("/Fields");
        for (int i = 0; i < fields.getArrayNItems(); ++i) {
            QPDFObjectHandle v = fields.getArrayItem(i).getKey("/V");
            if (!v.isDictionary() || !v.hasKey("/Type")
                || v.getKey("/Type").getName() != "/DocTimeStamp")
                continue;
            foundDts = true;
            QPDFObjectHandle br = v.getKey("/ByteRange");
            QVERIFY(br.isArray() && br.getArrayNItems() == 4);
            const long long start2 = br.getArrayItem(2).getIntValue();
            const long long len2 = br.getArrayItem(3).getIntValue();
            QCOMPARE(br.getArrayItem(0).getIntValue(), 0LL);
            QCOMPARE(start2 + len2, static_cast<long long>(fileSize)); // covers to EOF
            const std::string contents = v.getKey("/Contents").getStringValue();
            QVERIFY(!contents.empty());
            QByteArray c(contents.data(), static_cast<int>(contents.size()));
            QVERIFY(c.count('\0') < c.size()); // a real token, not all padding
        }
        QVERIFY(foundDts);
    }

private:
    static bool ToolLocatorHasNoOpenssl() { return ToolLocator::openssl().isEmpty(); }
};

QTEST_MAIN(TestSigner)
#include "test_signer.moc"
