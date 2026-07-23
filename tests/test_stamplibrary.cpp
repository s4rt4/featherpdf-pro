// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Tests for the stamp library: a preset stamp with date + name add-ons can be
// placed and read back as a /Stamp annotation whose caption appears in both its
// /Contents and its /AP appearance stream. The date/name are passed in so the
// asserted caption is deterministic (no reliance on "today").

#include "backends/StampLibrary.h"

#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

#include <qpdf/Buffer.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>

class TestStampLibrary : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_base;

    void makeSample(const QString& path) {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        painter.drawText(200, 300, QStringLiteral("Sample document"));
        painter.end();
    }

    // The first /Stamp annotation on page 0, or a null handle if there is none.
    QPDFObjectHandle firstStamp(QPDF& qpdf) {
        QPDFPageDocumentHelper dh(qpdf);
        std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
        QPDFObjectHandle annots = pages.at(0).getObjectHandle().getKey("/Annots");
        if (annots.isArray())
            for (int i = 0; i < annots.getArrayNItems(); ++i) {
                QPDFObjectHandle a = annots.getArrayItem(i);
                QPDFObjectHandle sub = a.getKey("/Subtype");
                if (sub.isName() && sub.getName() == "/Stamp")
                    return a;
            }
        return QPDFObjectHandle::newNull();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_base = m_dir.filePath("base.pdf");
        makeSample(m_base);
    }

    void captionComposesAddOns() {
        QCOMPARE(StampLibrary::caption(StampLibrary::Preset::Approved, QString(), QString()),
                 QStringLiteral("APPROVED"));
        QCOMPARE(StampLibrary::caption(StampLibrary::Preset::Confidential,
                                       QStringLiteral("2026-06-27"), QStringLiteral("Vinvan")),
                 QStringLiteral("CONFIDENTIAL - 2026-06-27 - Vinvan"));
        QCOMPARE(StampLibrary::caption(StampLibrary::Preset::ForReview, QString(),
                                       QStringLiteral("Vinvan")),
                 QStringLiteral("FOR REVIEW - Vinvan"));
    }

    void placeAndReadBack() {
        const QString out = m_dir.filePath("stamped.pdf");
        const QRectF rect(0.30, 0.20, 0.40, 0.10); // top-left, displayed fractions
        const QString date = QStringLiteral("2026-06-27");
        const QString name = QStringLiteral("Vinvan");
        const QString expected = QStringLiteral("CONFIDENTIAL - 2026-06-27 - Vinvan");

        QString err;
        QVERIFY2(StampLibrary::addStamp(m_base, out, 0, rect, StampLibrary::Preset::Confidential,
                                        date, name, &err),
                 qPrintable(err));

        QPDF qpdf;
        qpdf.processFile(out.toLocal8Bit().constData());
        QPDFObjectHandle stamp = firstStamp(qpdf);
        QVERIFY2(!stamp.isNull(), "no /Stamp annotation was written");

        // /Contents carries the exact caption (Unicode round-trip).
        QPDFObjectHandle contents = stamp.getKey("/Contents");
        QVERIFY(contents.isString());
        QCOMPARE(QString::fromStdString(contents.getUTF8Value()), expected);

        // The /AP appearance stream draws the caption — its ASCII parts are present.
        QPDFObjectHandle ap = stamp.getKey("/AP");
        QVERIFY(ap.isDictionary());
        QPDFObjectHandle n = ap.getKey("/N");
        QVERIFY(n.isStream());
        const std::shared_ptr<Buffer> buf = n.getStreamData();
        const QByteArray content(reinterpret_cast<const char*>(buf->getBuffer()),
                                 int(buf->getSize()));
        QVERIFY(content.contains("CONFIDENTIAL"));
        QVERIFY(content.contains("2026-06-27"));
        QVERIFY(content.contains("Vinvan"));
    }

    void rejectsBadPage() {
        const QString out = m_dir.filePath("nope.pdf");
        QString err;
        QVERIFY(!StampLibrary::addStamp(m_base, out, 9, QRectF(0.1, 0.1, 0.3, 0.1),
                                        StampLibrary::Preset::Draft, QString(), QString(), &err));
        QVERIFY(!err.isEmpty());
    }
};

QTEST_MAIN(TestStampLibrary)
#include "test_stamplibrary.moc"
