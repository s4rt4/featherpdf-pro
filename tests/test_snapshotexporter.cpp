// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Tests for rendering a rectangular region of a page to an image (the Snapshot
// tool). Renders a generated PDF with QPdfDocument and checks the cropped output.

#include "backends/SnapshotExporter.h"

#include <QImage>
#include <QPageSize>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

class TestSnapshotExporter : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_pdf;

    void makeSample(const QString& path) {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        painter.drawText(200, 300, QStringLiteral("Snapshot me"));
        painter.end();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_pdf = m_dir.filePath("sample.pdf");
        makeSample(m_pdf);
    }

    void cropsToRegionSize() {
        QPdfDocument doc;
        QCOMPARE(doc.load(m_pdf), QPdfDocument::Error::None);
        QCOMPARE(doc.pageCount(), 1);

        QString err;
        // Full page at this DPI, to compare the crop against.
        const QImage full =
            SnapshotExporter::renderRegion(&doc, 0, 0, QRectF(0, 0, 1, 1), 96, &err);
        QVERIFY2(!full.isNull(), qPrintable(err));
        QVERIFY(full.width() > 0 && full.height() > 0);

        // The top-left quarter should be roughly half the page in each dimension.
        const QImage quarter =
            SnapshotExporter::renderRegion(&doc, 0, 0, QRectF(0, 0, 0.5, 0.5), 96, &err);
        QVERIFY2(!quarter.isNull(), qPrintable(err));
        QCOMPARE(quarter.width(), qRound(0.5 * full.width()));
        QCOMPARE(quarter.height(), qRound(0.5 * full.height()));
    }

    void dpiScalesResolution() {
        QPdfDocument doc;
        QCOMPARE(doc.load(m_pdf), QPdfDocument::Error::None);
        QString err;
        const QImage lo = SnapshotExporter::renderRegion(&doc, 0, 0, QRectF(0, 0, 0.5, 0.5), 72, &err);
        const QImage hi =
            SnapshotExporter::renderRegion(&doc, 0, 0, QRectF(0, 0, 0.5, 0.5), 144, &err);
        QVERIFY(!lo.isNull() && !hi.isNull());
        QVERIFY(hi.width() > lo.width() * 3 / 2);
    }

    void rotationSwapsDimensions() {
        QPdfDocument doc;
        QCOMPARE(doc.load(m_pdf), QPdfDocument::Error::None);
        QString err;
        // A full-page region: A4 portrait is taller than wide; a 90° turn flips it.
        const QImage up = SnapshotExporter::renderRegion(&doc, 0, 0, QRectF(0, 0, 1, 1), 96, &err);
        const QImage side = SnapshotExporter::renderRegion(&doc, 0, 90, QRectF(0, 0, 1, 1), 96, &err);
        QVERIFY(!up.isNull() && !side.isNull());
        QVERIFY(up.height() > up.width());
        QVERIFY(side.width() > side.height());
    }

    void emptyRegionFails() {
        QPdfDocument doc;
        QCOMPARE(doc.load(m_pdf), QPdfDocument::Error::None);
        QString err;
        const QImage img = SnapshotExporter::renderRegion(&doc, 0, 0, QRectF(0.5, 0.5, 0, 0), 96, &err);
        QVERIFY(img.isNull());
        QVERIFY(!err.isEmpty());
    }

    void nullDocumentFails() {
        QString err;
        const QImage img =
            SnapshotExporter::renderRegion(nullptr, 0, 0, QRectF(0, 0, 1, 1), 96, &err);
        QVERIFY(img.isNull());
        QVERIFY(!err.isEmpty());
    }

    void outOfRangePageFails() {
        QPdfDocument doc;
        QCOMPARE(doc.load(m_pdf), QPdfDocument::Error::None);
        QString err;
        const QImage img = SnapshotExporter::renderRegion(&doc, 9, 0, QRectF(0, 0, 1, 1), 96, &err);
        QVERIFY(img.isNull());
        QVERIFY(!err.isEmpty());
    }
};

QTEST_MAIN(TestSnapshotExporter)
#include "test_snapshotexporter.moc"
