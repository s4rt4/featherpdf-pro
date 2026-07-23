// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Tests for header/footer stamping: tokens must expand per page and the text
// must be present and extractable from the output.

#include "backends/Watermarker.h"

#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include <poppler-qt6.h>

class TestHeaderFooter : public QObject {
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
            painter.drawText(200, 400, QStringLiteral("Body %1").arg(i + 1));
        }
        painter.end();
    }

    QString pageText(const QString& path, int page) {
        std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(path);
        if (!doc)
            return {};
        std::unique_ptr<Poppler::Page> p = doc->page(page);
        return p ? p->text(QRectF()) : QString();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_pdf = m_dir.filePath("sample.pdf");
        makeSample(m_pdf, 3);
    }

    void stampsPageNumbersWithTokens() {
        const QString out = m_dir.filePath("hf.pdf");
        Watermarker::HeaderFooterOptions o;
        o.footerCenter = QStringLiteral("Page {n} of {p}");
        o.headerLeft = QStringLiteral("Report");
        QString err;
        QVERIFY2(Watermarker::addHeaderFooter(m_pdf, out, o, &err), qPrintable(err));

        const QString p0 = pageText(out, 0);
        const QString p2 = pageText(out, 2);
        QVERIFY2(p0.contains(QStringLiteral("Page 1 of 3")), qPrintable(p0));
        QVERIFY2(p2.contains(QStringLiteral("Page 3 of 3")), qPrintable(p2));
        QVERIFY(p0.contains(QStringLiteral("Report")));
        // Original content survives.
        QVERIFY(p0.contains(QStringLiteral("Body 1")));
    }

    void startNumberOffsetsPages() {
        const QString out = m_dir.filePath("hf2.pdf");
        Watermarker::HeaderFooterOptions o;
        o.footerRight = QStringLiteral("{n}");
        o.startNumber = 10;
        QString err;
        QVERIFY2(Watermarker::addHeaderFooter(m_pdf, out, o, &err), qPrintable(err));
        QVERIFY(pageText(out, 0).contains(QStringLiteral("10")));
        QVERIFY(pageText(out, 2).contains(QStringLiteral("12")));
    }

    void emptyOptionsFail() {
        const QString out = m_dir.filePath("hf3.pdf");
        Watermarker::HeaderFooterOptions o; // all slots empty
        QString err;
        QVERIFY(!Watermarker::addHeaderFooter(m_pdf, out, o, &err));
        QVERIFY(!err.isEmpty());
    }
};

QTEST_MAIN(TestHeaderFooter)
#include "test_headerfooter.moc"
