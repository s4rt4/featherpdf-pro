// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Tests for PDF/A-1b tagging + preflight: tagging a clean PDF must write a
// GTS_PDFA1 sRGB OutputIntent and an XMP packet declaring pdfaid:part/conformance,
// and the optional veraPDF preflight must degrade gracefully when absent.

#include "backends/PdfA.h"

#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <qpdf/Buffer.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>

class TestPdfA : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_base;

    void makeBase(const QString& path) {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        painter.drawText(200, 300, QStringLiteral("Visible content"));
        painter.end();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_base = m_dir.filePath("base.pdf");
        makeBase(m_base);
    }

    // Tagging writes the OutputIntent, XMP, MarkInfo and /ID.
    void tagsTowardPdfA1b() {
        const QString out = m_dir.filePath("tagged.pdf");
        QString err;
        QVERIFY2(PdfA::convertToPdfA1b(m_base, out, &err), qPrintable(err));

        QPDF q;
        q.processFile(out.toLocal8Bit().constData());
        QPDFObjectHandle root = q.getRoot();

        // --- OutputIntent with GTS_PDFA1 + embedded sRGB profile ---
        QPDFObjectHandle ois = root.getKey("/OutputIntents");
        QVERIFY(ois.isArray());
        QVERIFY(ois.getArrayNItems() >= 1);
        QPDFObjectHandle oi = ois.getArrayItem(0);
        QVERIFY(oi.isDictionary());
        QPDFObjectHandle s = oi.getKey("/S");
        QVERIFY(s.isName());
        QCOMPARE(QString::fromStdString(s.getName()), QStringLiteral("/GTS_PDFA1"));
        QPDFObjectHandle oci = oi.getKey("/OutputConditionIdentifier");
        QVERIFY(oci.isString());
        QCOMPARE(QString::fromStdString(oci.getUTF8Value()), QStringLiteral("sRGB"));
        QPDFObjectHandle dest = oi.getKey("/DestOutputProfile");
        QVERIFY(dest.isStream());
        QPDFObjectHandle destDict = dest.getDict();
        QVERIFY(destDict.getKey("/N").isInteger());
        QCOMPARE(destDict.getKey("/N").getIntValue(), 3LL);
        // The embedded stream must actually carry ICC profile bytes.
        QVERIFY(dest.getStreamData()->getSize() > 0);

        // --- MarkInfo /Marked true ---
        QPDFObjectHandle markInfo = root.getKey("/MarkInfo");
        QVERIFY(markInfo.isDictionary());
        QVERIFY(markInfo.getKey("/Marked").isBool());
        QVERIFY(markInfo.getKey("/Marked").getBoolValue());

        // --- /ID present in the trailer ---
        QVERIFY(q.getTrailer().getKey("/ID").isArray());

        // --- XMP packet declares PDF/A-1b ---
        QPDFObjectHandle meta = root.getKey("/Metadata");
        QVERIFY(meta.isStream());
        const std::shared_ptr<Buffer> buf = meta.getStreamData();
        const QByteArray xmp(reinterpret_cast<const char*>(buf->getBuffer()),
                             static_cast<int>(buf->getSize()));
        QVERIFY2(xmp.contains("pdfaid:part"), "XMP missing pdfaid:part");
        QVERIFY2(xmp.contains("pdfaid:conformance"), "XMP missing pdfaid:conformance");
        QVERIFY2(xmp.contains(">1<"), "pdfaid:part should be 1");
        QVERIFY2(xmp.contains(">B<"), "pdfaid:conformance should be B");
    }

    // Preflight must work whether or not veraPDF is installed.
    void preflightDegradesGracefully() {
        const QString out = m_dir.filePath("tagged2.pdf");
        QString err;
        QVERIFY2(PdfA::convertToPdfA1b(m_base, out, &err), qPrintable(err));

        const bool installed = !QStandardPaths::findExecutable(QStringLiteral("verapdf")).isEmpty();
        QCOMPARE(PdfA::hasValidator(), installed);

        PdfA::PreflightReport rep;
        QString verr;
        // validate() returns true in both the present and absent cases.
        QVERIFY2(PdfA::validate(out, &rep, &verr), qPrintable(verr));

        if (installed) {
            QVERIFY(rep.available);
            QVERIFY(rep.ran);                // veraPDF produced a verdict
            QVERIFY(!rep.profile.isEmpty()); // e.g. PDF/A-1B
        } else {
            QVERIFY(!rep.available);
            QVERIFY(!rep.summary.isEmpty()); // a clear "not installed" message
            QVERIFY(rep.summary.contains(QStringLiteral("veraPDF")));
        }
    }
};

QTEST_MAIN(TestPdfA)
#include "test_pdfa.moc"
