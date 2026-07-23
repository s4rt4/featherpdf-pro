// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Tests for the "Export to editable" path. The plain-text export runs in-process
// via Poppler, so it is deterministic and CI-friendly; the LibreOffice-backed
// docx/odt paths need soffice and produce non-deterministic layout, so they are
// only smoke-tested when soffice happens to be installed.

#include "backends/Converter.h"

#include <QFile>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

class TestConverter : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_pdf;

    void makeSample(const QString& path) {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        painter.drawText(200, 300, QStringLiteral("AlphaUnique line one"));
        painter.drawText(200, 600, QStringLiteral("BetaUnique line two"));
        painter.end();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_pdf = m_dir.filePath("sample.pdf");
        makeSample(m_pdf);
        QVERIFY(QFile::exists(m_pdf));
    }

    // .txt routes through Poppler and must reproduce the page text exactly once.
    void exportsCleanText() {
        const QString out = m_dir.filePath("sample.txt");
        QString err;
        QVERIFY2(Converter::pdfToOffice(m_pdf, out, &err), qPrintable(err));
        QVERIFY(QFile::exists(out));

        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(f.readAll());
        QVERIFY2(text.contains(QStringLiteral("AlphaUnique line one")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("BetaUnique line two")), qPrintable(text));
        // Poppler extraction is clean: each line appears exactly once (unlike the
        // LibreOffice Writer import, which doubles lines).
        QCOMPARE(text.count(QStringLiteral("AlphaUnique")), 1);
    }

    // An extension we don't support must fail cleanly rather than silently.
    void rejectsUnsupportedFormat() {
        const QString out = m_dir.filePath("sample.xyz");
        QString err;
        QVERIFY(!Converter::pdfToOffice(m_pdf, out, &err));
        QVERIFY(!err.isEmpty());
    }

    // Smoke test: when soffice is available, the Word path produces a non-empty file.
    void exportsWordWhenLibreOfficePresent() {
        if (!Converter::hasOfficeConverter())
            QSKIP("LibreOffice not installed");
        const QString out = m_dir.filePath("sample.docx");
        QString err;
        QVERIFY2(Converter::pdfToOffice(m_pdf, out, &err), qPrintable(err));
        QVERIFY(QFile::exists(out));
        QVERIFY(QFileInfo(out).size() > 0);
    }
};

QTEST_MAIN(TestConverter)
#include "test_converter.moc"
