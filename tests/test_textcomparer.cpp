// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Tests for word-level text comparison: added/removed words are detected per
// page, identical documents report no change, and extra pages count as added.

#include "backends/TextComparer.h"

#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

class TestTextComparer : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString makeDoc(const QString& name, const QStringList& pages) {
        const QString path = m_dir.filePath(name);
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        for (int i = 0; i < pages.size(); ++i) {
            if (i)
                writer.newPage();
            painter.drawText(200, 400, pages[i]);
        }
        painter.end();
        return path;
    }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void detectsAddedAndRemovedWords() {
        const QString a = makeDoc("a.pdf", {QStringLiteral("The quick brown fox")});
        const QString b = makeDoc("b.pdf", {QStringLiteral("The slow brown fox jumps")});
        QString err;
        const TextComparer::Result r = TextComparer::compare(a, b, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QVERIFY(r.changed());
        QCOMPARE(r.pages.size(), 1);
        QCOMPARE(r.pages[0].page, 0);
        QVERIFY2(r.pages[0].removed.contains(QStringLiteral("quick")),
                 qPrintable(r.pages[0].removed.join(',')));
        QVERIFY(r.pages[0].added.contains(QStringLiteral("slow")));
        QVERIFY(r.pages[0].added.contains(QStringLiteral("jumps")));
        QVERIFY(!r.pages[0].removed.contains(QStringLiteral("brown"))); // unchanged
    }

    void identicalDocumentsReportNoChange() {
        const QString a = makeDoc("c.pdf", {QStringLiteral("Same words here")});
        const QString b = makeDoc("d.pdf", {QStringLiteral("Same words here")});
        QString err;
        const TextComparer::Result r = TextComparer::compare(a, b, &err);
        QVERIFY(!r.changed());
        QCOMPARE(r.addedWords, 0);
        QCOMPARE(r.removedWords, 0);
    }

    void extraPageCountsAsAdded() {
        const QString a = makeDoc("e.pdf", {QStringLiteral("Page one text")});
        const QString b = makeDoc("f.pdf",
                                  {QStringLiteral("Page one text"), QStringLiteral("Brand new page")});
        QString err;
        const TextComparer::Result r = TextComparer::compare(a, b, &err);
        QCOMPARE(r.pageCount, 2);
        QCOMPARE(r.pages.size(), 1);
        QCOMPARE(r.pages[0].page, 1);
        QVERIFY(r.pages[0].added.contains(QStringLiteral("Brand")));
        QVERIFY(r.pages[0].removed.isEmpty());
    }
};

QTEST_MAIN(TestTextComparer)
#include "test_textcomparer.moc"
