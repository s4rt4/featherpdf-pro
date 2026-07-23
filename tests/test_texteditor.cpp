// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Tests for M8 stage (a): editing the text you added. A /FreeText box created
// with the annotator must be readable, re-writable, and removable by TextEditor.

#include "backends/Annotator.h"
#include "backends/TextEditor.h"

#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRectF>
#include <QTemporaryDir>
#include <QtTest>

class TestTextEditor : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_withText;

    void makeSample(const QString& path) {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        painter.drawText(100, 100, QStringLiteral("Page 1"));
        painter.end();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        const QString base = m_dir.filePath("base.pdf");
        makeSample(base);
        m_withText = m_dir.filePath("withtext.pdf");
        Annotator::Shape box{0, Annotator::Shape::Kind::TextBox, QRectF(0.1, 0.1, 0.4, 0.08),
                             QColor(Qt::black)};
        box.text = "hello";
        QString err;
        QVERIFY2(Annotator::saveAnnotations(base, m_withText, {}, {}, {}, {box}, &err),
                 qPrintable(err));
    }

    void readsAddedTextBox() {
        const QList<TextEditor::TextBox> boxes = TextEditor::read(m_withText);
        QCOMPARE(boxes.size(), 1);
        QCOMPARE(boxes[0].page, 0);
        QCOMPARE(boxes[0].text, QStringLiteral("hello"));
    }

    void editsTextBox() {
        const QList<TextEditor::TextBox> boxes = TextEditor::read(m_withText);
        QCOMPARE(boxes.size(), 1);
        const QString out = m_dir.filePath("edited.pdf");
        QString err;
        QVERIFY2(TextEditor::setText(m_withText, out, boxes[0].objId, boxes[0].objGen, "world",
                                     &err),
                 qPrintable(err));
        const QList<TextEditor::TextBox> after = TextEditor::read(out);
        QCOMPARE(after.size(), 1);
        QCOMPARE(after[0].text, QStringLiteral("world"));
    }

    void removesTextBox() {
        const QList<TextEditor::TextBox> boxes = TextEditor::read(m_withText);
        const QString out = m_dir.filePath("removed.pdf");
        QString err;
        QVERIFY2(TextEditor::remove(m_withText, out, boxes[0].objId, boxes[0].objGen, &err),
                 qPrintable(err));
        QCOMPARE(TextEditor::read(out).size(), 0);
    }

    void rejectsUnknownHandle() {
        const QString out = m_dir.filePath("nope.pdf");
        QString err;
        QVERIFY(!TextEditor::setText(m_withText, out, 99999, 0, "x", &err));
        QVERIFY(!err.isEmpty());
    }
};

QTEST_MAIN(TestTextEditor)
#include "test_texteditor.moc"
