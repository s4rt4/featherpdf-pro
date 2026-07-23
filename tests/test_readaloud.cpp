// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Headless tests for the Read-aloud text backend (ReadAloud / Poppler): a
// document is split into sentence-ish utterances in reading order, each tagged
// with its page. No audio is involved — only the extraction/chunking logic.

#include "backends/ReadAloud.h"

#include <QFont>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

class TestReadAloud : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    // A two-page document; page 1 has three sentences, page 2 has one.
    QString makeDoc() {
        const QString path = m_dir.filePath("doc.pdf");
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setResolution(72);
        QPainter p(&writer);
        QFont f = p.font();
        f.setPointSize(12);
        p.setFont(f);
        p.drawText(72, 100, QStringLiteral("Hello world. This is a test!"));
        p.drawText(72, 130, QStringLiteral("Is it working?"));
        writer.newPage();
        p.drawText(72, 100, QStringLiteral("The second page has one line."));
        p.end();
        return path;
    }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void splitsSentences() {
        const QStringList s =
            ReadAloud::splitSentences(QStringLiteral("Hello world. This is a test! Is it working?"));
        QCOMPARE(s.size(), 3);
        QCOMPARE(s.at(0), QStringLiteral("Hello world."));
        QCOMPARE(s.at(1), QStringLiteral("This is a test!"));
        QCOMPARE(s.at(2), QStringLiteral("Is it working?"));
    }

    void normalisesWhitespace() {
        // Newlines and runs of spaces (as Poppler emits mid-paragraph) collapse.
        const QStringList s =
            ReadAloud::splitSentences(QStringLiteral("A wrapped\nsentence   here. Next one."));
        QCOMPARE(s.size(), 2);
        QCOMPARE(s.at(0), QStringLiteral("A wrapped sentence here."));
    }

    void dropsNonWordChunks() {
        // A row of dashes / pure punctuation isn't worth speaking.
        const QStringList s = ReadAloud::splitSentences(QStringLiteral("--------. Real text."));
        QCOMPARE(s.size(), 1);
        QCOMPARE(s.at(0), QStringLiteral("Real text."));
    }

    void emptyTextYieldsNothing() {
        QVERIFY(ReadAloud::splitSentences(QString()).isEmpty());
        QVERIFY(ReadAloud::splitSentences(QStringLiteral("   \n  ")).isEmpty());
    }

    void extractsUtterancesWithPages() {
        const QString doc = makeDoc();
        QString err;
        const QList<ReadAloud::Utterance> u = ReadAloud::utterances(doc, &err);
        QVERIFY2(!u.isEmpty(), qPrintable(err));
        QVERIFY(err.isEmpty());

        // Every utterance carries non-empty text and a valid page index, in order.
        int lastPage = 0;
        bool sawPage1 = false;
        for (const ReadAloud::Utterance& x : u) {
            QVERIFY(!x.text.trimmed().isEmpty());
            QVERIFY(x.page >= lastPage); // reading order never goes backwards
            lastPage = x.page;
            if (x.page == 1)
                sawPage1 = true;
        }
        QVERIFY2(sawPage1, "expected at least one utterance from the second page");
    }

    void missingFileReportsError() {
        QString err;
        const QList<ReadAloud::Utterance> u =
            ReadAloud::utterances(m_dir.filePath("nope.pdf"), &err);
        QVERIFY(u.isEmpty());
        QVERIFY(!err.isEmpty());
    }
};

QTEST_MAIN(TestReadAloud)
#include "test_readaloud.moc"
