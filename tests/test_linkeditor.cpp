// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Tests for the link editor: a URL link can be added, read back with its target
// and position, have its URL changed, and be deleted.

#include "backends/LinkEditor.h"

#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

class TestLinkEditor : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_base;

    void makeSample(const QString& path) {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        painter.drawText(200, 300, QStringLiteral("Click here"));
        painter.end();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_base = m_dir.filePath("base.pdf");
        makeSample(m_base);
    }

    void addReadEditDelete() {
        const QString withLink = m_dir.filePath("withlink.pdf");
        const QRectF rect(0.15, 0.20, 0.30, 0.06); // top-left, displayed fractions
        QString err;
        QVERIFY2(LinkEditor::addUriLink(m_base, withLink, 0, rect,
                                        QStringLiteral("https://example.com"), &err),
                 qPrintable(err));

        QList<LinkEditor::Link> links = LinkEditor::read(withLink);
        QCOMPARE(links.size(), 1);
        QCOMPARE(links[0].page, 0);
        QVERIFY(links[0].isUri);
        QCOMPARE(links[0].uri, QStringLiteral("https://example.com"));
        // The rect should round-trip to roughly the same fractions.
        QVERIFY(qAbs(links[0].rect.x() - rect.x()) < 0.02);
        QVERIFY(qAbs(links[0].rect.y() - rect.y()) < 0.02);
        QVERIFY(qAbs(links[0].rect.width() - rect.width()) < 0.02);
        QVERIFY(qAbs(links[0].rect.height() - rect.height()) < 0.02);

        // Change the URL.
        const QString edited = m_dir.filePath("edited.pdf");
        LinkEditor::Edit change;
        change.objId = links[0].objId;
        change.objGen = links[0].objGen;
        change.newUri = QStringLiteral("https://feather.example/new");
        QVERIFY2(LinkEditor::applyEdits(withLink, edited, {change}, &err), qPrintable(err));
        links = LinkEditor::read(edited);
        QCOMPARE(links.size(), 1);
        QCOMPARE(links[0].uri, QStringLiteral("https://feather.example/new"));

        // Delete it.
        const QString removed = m_dir.filePath("removed.pdf");
        LinkEditor::Edit del;
        del.objId = links[0].objId;
        del.objGen = links[0].objGen;
        del.remove = true;
        QVERIFY2(LinkEditor::applyEdits(edited, removed, {del}, &err), qPrintable(err));
        QVERIFY(LinkEditor::read(removed).isEmpty());
    }

    void rejectsEmptyUrl() {
        const QString out = m_dir.filePath("nope.pdf");
        QString err;
        QVERIFY(!LinkEditor::addUriLink(m_base, out, 0, QRectF(0.1, 0.1, 0.2, 0.05),
                                        QStringLiteral("   "), &err));
        QVERIFY(!err.isEmpty());
    }

    void readEmptyWhenNoLinks() { QVERIFY(LinkEditor::read(m_base).isEmpty()); }
};

QTEST_MAIN(TestLinkEditor)
#include "test_linkeditor.moc"
