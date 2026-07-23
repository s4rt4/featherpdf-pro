// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Tests for Find & Redact: a regex over a page's text must come back with the
// bounding boxes (normalised) of the matching words.

#include "backends/PatternRedactor.h"

#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

class TestPatternRedactor : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_pdf;

    void makeSample(const QString& path) {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        QFont f = painter.font();
        f.setPointSize(14);
        painter.setFont(f);
        painter.drawText(300, 400, QStringLiteral("Email alice@example.com here"));
        painter.drawText(300, 800, QStringLiteral("KTP 1234567890123456 secret"));
        painter.end();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_pdf = m_dir.filePath("sample.pdf");
        makeSample(m_pdf);
    }

    void findsEmail() {
        int total = 0;
        QString err;
        const auto hits = PatternRedactor::findMatches(
            m_pdf, QRegularExpression(QStringLiteral("[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}")),
            &total, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QVERIFY2(total >= 1, "expected at least one email match");
        QVERIFY(hits.contains(0));
        QVERIFY(!hits.value(0).isEmpty());
        // Rects are normalised inside the page.
        for (const QRectF& r : hits.value(0)) {
            QVERIFY(r.x() >= 0.0 && r.x() <= 1.0);
            QVERIFY(r.y() >= 0.0 && r.y() <= 1.0);
            QVERIFY(r.width() > 0.0 && r.height() > 0.0);
        }
    }

    void findsSixteenDigitId() {
        int total = 0;
        QString err;
        const auto hits = PatternRedactor::findMatches(
            m_pdf, QRegularExpression(QStringLiteral("\\b\\d{16}\\b")), &total, &err);
        QVERIFY2(total >= 1, "expected the 16-digit id to match");
        QVERIFY(hits.contains(0));
    }

    void noMatchIsEmptyNotError() {
        int total = 0;
        QString err;
        const auto hits = PatternRedactor::findMatches(
            m_pdf, QRegularExpression(QStringLiteral("ZZZ-not-present-QQQ")), &total, &err);
        QVERIFY(err.isEmpty());
        QCOMPARE(total, 0);
        QVERIFY(hits.isEmpty());
    }

    void missingFileErrors() {
        int total = 0;
        QString err;
        const auto hits = PatternRedactor::findMatches(
            m_dir.filePath("nope.pdf"), QRegularExpression(QStringLiteral("x")), &total, &err);
        QVERIFY(hits.isEmpty());
        QVERIFY(!err.isEmpty());
    }
};

QTEST_MAIN(TestPatternRedactor)
#include "test_patternredactor.moc"
