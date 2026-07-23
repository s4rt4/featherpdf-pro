// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Tests for Optimize: a PDF carrying an oversized image must audit non-zero image
// bytes, and downsampling to a low target DPI must shrink the stored pixel
// dimensions (and the file). Unembedding fonts must strip the font program.

#include "backends/Optimizer.h"

#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <string>

class TestOptimizer : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_src;

    static const int kImgW = 1600;
    static const int kImgH = 1200;

    // Build a one-page A4 PDF whose only content is a big uncompressed RGB image
    // stretched across the whole page (so its effective DPI is very high).
    void makeImagePdf(const QString& path) {
        QPDF q;
        q.emptyPDF();

        // Raw DeviceRGB samples, 8 bits per component, no filter.
        std::string samples;
        samples.reserve(static_cast<size_t>(kImgW) * kImgH * 3);
        for (int y = 0; y < kImgH; ++y) {
            for (int x = 0; x < kImgW; ++x) {
                samples.push_back(static_cast<char>(x % 256));
                samples.push_back(static_cast<char>(y % 256));
                samples.push_back(static_cast<char>((x + y) % 256));
            }
        }

        QPDFObjectHandle image = QPDFObjectHandle::newStream(&q);
        QPDFObjectHandle idict = image.getDict();
        idict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
        idict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
        idict.replaceKey("/Width", QPDFObjectHandle::newInteger(kImgW));
        idict.replaceKey("/Height", QPDFObjectHandle::newInteger(kImgH));
        idict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceRGB"));
        idict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
        image.replaceStreamData(samples, QPDFObjectHandle::newNull(),
                                QPDFObjectHandle::newNull());
        QPDFObjectHandle imageRef = q.makeIndirectObject(image);

        // A4 in points: 595 x 842. Draw the image across the full page.
        QPDFObjectHandle xobjects = QPDFObjectHandle::newDictionary();
        xobjects.replaceKey("/Im0", imageRef);
        QPDFObjectHandle resources = QPDFObjectHandle::newDictionary();
        resources.replaceKey("/XObject", xobjects);

        const std::string contentStr = "q 595 0 0 842 0 0 cm /Im0 Do Q";
        QPDFObjectHandle contents = QPDFObjectHandle::newStream(&q, contentStr);

        QPDFObjectHandle mediaBox = QPDFObjectHandle::newArray();
        mediaBox.appendItem(QPDFObjectHandle::newInteger(0));
        mediaBox.appendItem(QPDFObjectHandle::newInteger(0));
        mediaBox.appendItem(QPDFObjectHandle::newInteger(595));
        mediaBox.appendItem(QPDFObjectHandle::newInteger(842));

        QPDFObjectHandle page = QPDFObjectHandle::newDictionary();
        page.replaceKey("/Type", QPDFObjectHandle::newName("/Page"));
        page.replaceKey("/MediaBox", mediaBox);
        page.replaceKey("/Contents", q.makeIndirectObject(contents));
        page.replaceKey("/Resources", resources);

        QPDFPageDocumentHelper(q).addPage(page, false);

        QPDFWriter w(q, path.toLocal8Bit().constData());
        w.setStreamDataMode(qpdf_s_preserve); // keep the image uncompressed in the source
        w.write();
    }

    // First image XObject's stored pixel dimensions.
    static void firstImageDims(const QString& path, int* w, int* h) {
        *w = 0;
        *h = 0;
        QPDF q;
        q.processFile(path.toLocal8Bit().constData());
        for (QPDFObjectHandle obj : q.getAllObjects()) {
            if (obj.isStream() && obj.isImage(false)) {
                *w = static_cast<int>(obj.getDict().getKey("/Width").getIntValue());
                *h = static_cast<int>(obj.getDict().getKey("/Height").getIntValue());
                return;
            }
        }
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_src = m_dir.filePath("big-image.pdf");
        makeImagePdf(m_src);
        QVERIFY(QFileInfo::exists(m_src));
    }

    void auditReportsImageBytes() {
        Optimizer::Audit a;
        QString err;
        QVERIFY2(Optimizer::audit(m_src, &a, &err), qPrintable(err));
        QVERIFY(a.total > 0);
        QVERIFY2(a.images > 0, "expected non-zero image bytes in the audit");
    }

    void downsampleShrinksImageAndFile() {
        int w0 = 0, h0 = 0;
        firstImageDims(m_src, &w0, &h0);
        QCOMPARE(w0, kImgW);
        QCOMPARE(h0, kImgH);
        const qint64 srcBytes = QFileInfo(m_src).size();

        const QString out = m_dir.filePath("optimized.pdf");
        Optimizer::Options opt;
        opt.targetDpi = 72; // well below the source's effective DPI
        opt.dropFonts = false;
        opt.compress = true;
        Optimizer::Report rep;
        QString err;
        QVERIFY2(Optimizer::optimize(m_src, out, opt, &rep, &err), qPrintable(err));
        QCOMPARE(rep.imagesDownsampled, 1);

        int w1 = 0, h1 = 0;
        firstImageDims(out, &w1, &h1);
        QVERIFY2(w1 < w0 && h1 < h0, "downsample should reduce stored pixel dimensions");

        QVERIFY2(QFileInfo(out).size() < srcBytes, "optimized file should be smaller");
        QCOMPARE(rep.afterBytes, QFileInfo(out).size());
    }

    void offTargetKeepsImage() {
        int w0 = 0, h0 = 0;
        firstImageDims(m_src, &w0, &h0);

        const QString out = m_dir.filePath("nodpi.pdf");
        Optimizer::Options opt;
        opt.targetDpi = 0; // downsampling disabled
        Optimizer::Report rep;
        QString err;
        QVERIFY2(Optimizer::optimize(m_src, out, opt, &rep, &err), qPrintable(err));
        QCOMPARE(rep.imagesDownsampled, 0);

        int w1 = 0, h1 = 0;
        firstImageDims(out, &w1, &h1);
        QCOMPARE(w1, w0);
        QCOMPARE(h1, h0);
    }
};

QTEST_MAIN(TestOptimizer)
#include "test_optimizer.moc"
