// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Headless tests for the façade's page edit model (FeatherDocument): the slot
// arrangement that drives rotate / delete / reorder.

#include "core/FeatherDocument.h"

#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

class TestDocument : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_input;

    void makeSample(const QString& path, int pages) {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        for (int i = 0; i < pages; ++i) {
            if (i > 0)
                writer.newPage();
            painter.drawText(100, 100, QStringLiteral("Page %1").arg(i + 1));
        }
        painter.end();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_input = m_dir.filePath("input.pdf");
        makeSample(m_input, 3);
    }

    void loadsIdentityArrangement() {
        FeatherDocument doc;
        QCOMPARE(doc.load(m_input), FeatherDocument::LoadResult::Ok);
        QCOMPARE(doc.pageCount(), 3);
        QCOMPARE(doc.originalPageCount(), 3);
        QCOMPARE(doc.pageOrder(), (QVector<int>{0, 1, 2}));
        QCOMPARE(doc.rotations(), (QVector<int>{0, 0, 0}));
        QVERIFY(!doc.isModified());
    }

    void rotateNormalisesAndMarksModified() {
        FeatherDocument doc;
        QCOMPARE(doc.load(m_input), FeatherDocument::LoadResult::Ok);
        doc.rotatePage(1, 90);
        QCOMPARE(doc.rotation(1), 90);
        doc.rotatePage(1, 300); // 90 + 300 = 390 → 30? no: normalised to 30... 390%360=30
        QCOMPARE(doc.rotation(1), 30);
        doc.rotatePage(1, -30); // back to 0
        QCOMPARE(doc.rotation(1), 0);
        QVERIFY(doc.isModified());
        doc.markSaved();
        QVERIFY(!doc.isModified());
    }

    void removeThenInsertRestoresOrder() {
        FeatherDocument doc;
        QCOMPARE(doc.load(m_input), FeatherDocument::LoadResult::Ok);
        doc.rotatePage(0, 90);

        int orig = -1, rot = -1;
        doc.removePage(0, &orig, &rot);
        QCOMPARE(orig, 0);
        QCOMPARE(rot, 90);
        QCOMPARE(doc.pageCount(), 2);
        QCOMPARE(doc.pageOrder(), (QVector<int>{1, 2}));

        doc.insertPage(0, orig, rot); // undo of the delete
        QCOMPARE(doc.pageCount(), 3);
        QCOMPARE(doc.pageOrder(), (QVector<int>{0, 1, 2}));
        QCOMPARE(doc.rotation(0), 90);
    }

    void moveReordersSlots() {
        FeatherDocument doc;
        QCOMPARE(doc.load(m_input), FeatherDocument::LoadResult::Ok);
        doc.movePage(0, 2);
        QCOMPARE(doc.pageOrder(), (QVector<int>{1, 2, 0}));
        QCOMPARE(doc.originalPageAt(2), 0);
        doc.movePage(2, 0); // undo
        QCOMPARE(doc.pageOrder(), (QVector<int>{0, 1, 2}));
    }
};

QTEST_MAIN(TestDocument)
#include "test_document.moc"
