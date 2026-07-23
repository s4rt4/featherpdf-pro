// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Tests for rendering pages to image files (the "Export Pages as Images" path).
// Renders a generated PDF with QPdfDocument and checks the output files.

#include "backends/ImageExporter.h"

#include <QImage>
#include <QPageSize>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

class TestImageExporter : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_pdf;

    void makeSample(const QString& path, int pages) {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        for (int i = 0; i < pages; ++i) {
            if (i)
                writer.newPage();
            painter.drawText(200, 300, QStringLiteral("Page %1").arg(i + 1));
        }
        painter.end();
    }

    // A PDF that embeds an actual raster image (an XObject pdfimages can pull out).
    void makeWithImage(const QString& path) {
        QImage pic(64, 48, QImage::Format_RGB32);
        pic.fill(Qt::darkCyan);
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        painter.drawImage(QRect(500, 500, 2000, 1500), pic);
        painter.end();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_pdf = m_dir.filePath("sample.pdf");
        makeSample(m_pdf, 3);
    }

    void rendersSelectedPages() {
        QPdfDocument doc;
        QCOMPARE(doc.load(m_pdf), QPdfDocument::Error::None);
        QCOMPARE(doc.pageCount(), 3);

        QTemporaryDir out;
        QVERIFY(out.isValid());
        // Pages 1 and 3 (0-based 0 and 2), labelled by display page number.
        const QVector<ImageExporter::PageSpec> specs{{0, 0, 1}, {2, 0, 3}};
        QString err;
        const int n = ImageExporter::renderPages(&doc, specs, out.path(),
                                                 QStringLiteral("doc"),
                                                 ImageExporter::Format::Png, 96, &err);
        QCOMPARE(n, 2);
        QVERIFY2(QFile::exists(out.filePath("doc-01.png")), "page 1 image missing");
        QVERIFY2(QFile::exists(out.filePath("doc-03.png")), "page 3 image missing");
        QVERIFY(!QFile::exists(out.filePath("doc-02.png")));

        // A real, non-empty raster.
        QImage img(out.filePath("doc-01.png"));
        QVERIFY(!img.isNull());
        QVERIFY(img.width() > 0 && img.height() > 0);
    }

    void dpiScalesResolution() {
        QPdfDocument doc;
        QCOMPARE(doc.load(m_pdf), QPdfDocument::Error::None);

        QTemporaryDir lo, hi;
        QString err;
        ImageExporter::renderPages(&doc, {{0, 0, 1}}, lo.path(), QStringLiteral("d"),
                                   ImageExporter::Format::Png, 72, &err);
        ImageExporter::renderPages(&doc, {{0, 0, 1}}, hi.path(), QStringLiteral("d"),
                                   ImageExporter::Format::Png, 144, &err);
        const QImage a(lo.filePath("d-01.png"));
        const QImage b(hi.filePath("d-01.png"));
        QVERIFY(!a.isNull() && !b.isNull());
        // Twice the DPI => roughly twice the pixels per side.
        QVERIFY(b.width() > a.width() * 3 / 2);
    }

    void rotationSwapsDimensions() {
        QPdfDocument doc;
        QCOMPARE(doc.load(m_pdf), QPdfDocument::Error::None);

        QTemporaryDir up, side;
        QString err;
        ImageExporter::renderPages(&doc, {{0, 0, 1}}, up.path(), QStringLiteral("r"),
                                   ImageExporter::Format::Png, 96, &err);
        ImageExporter::renderPages(&doc, {{0, 90, 1}}, side.path(), QStringLiteral("r"),
                                   ImageExporter::Format::Png, 96, &err);
        const QImage portrait(up.filePath("r-01.png"));
        const QImage landscape(side.filePath("r-01.png"));
        QVERIFY(!portrait.isNull() && !landscape.isNull());
        // A4 portrait is taller than wide; a 90° turn makes it wider than tall.
        QVERIFY(portrait.height() > portrait.width());
        QVERIFY(landscape.width() > landscape.height());
    }

    void extractsEmbeddedImages() {
        if (!ImageExporter::hasImageExtractor())
            QSKIP("pdfimages not installed");
        const QString withImg = m_dir.filePath("withimg.pdf");
        makeWithImage(withImg);

        QTemporaryDir out;
        QVERIFY(out.isValid());
        QString err;
        const int n = ImageExporter::extractEmbedded(withImg, out.path(), QStringLiteral("img"),
                                                     &err);
        QVERIFY2(n >= 1, qPrintable(QStringLiteral("expected >=1 image, got %1 (%2)")
                                        .arg(n)
                                        .arg(err)));
        const QStringList files = QDir(out.path()).entryList(QStringList{"img-*"}, QDir::Files);
        QVERIFY(!files.isEmpty());
    }

    void extractFromImagelessPdfFindsNothing() {
        if (!ImageExporter::hasImageExtractor())
            QSKIP("pdfimages not installed");
        // m_pdf is text-only - no embedded raster XObjects.
        QTemporaryDir out;
        QString err;
        const int n = ImageExporter::extractEmbedded(m_pdf, out.path(), QStringLiteral("none"),
                                                     &err);
        QCOMPARE(n, 0);
        QVERIFY2(err.isEmpty(), qPrintable(err)); // "none found" is not an error
    }

    void emptySelectionFails() {
        QPdfDocument doc;
        QCOMPARE(doc.load(m_pdf), QPdfDocument::Error::None);
        QString err;
        const int n = ImageExporter::renderPages(&doc, {}, m_dir.path(), QStringLiteral("x"),
                                                 ImageExporter::Format::Png, 96, &err);
        QCOMPARE(n, 0);
        QVERIFY(!err.isEmpty());
    }
};

QTEST_MAIN(TestImageExporter)
#include "test_imageexporter.moc"
