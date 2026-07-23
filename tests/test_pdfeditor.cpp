// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Headless tests for the lossless page-arrangement backend (PdfEditor / QPDF):
// rotate, delete, and reorder must produce the right page count and rotations.

#include "backends/PdfEditor.h"

#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFOutlineObjectHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>

#include <functional>
#include <vector>

class TestPdfEditor : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_input;
    QString m_source; // a separate 2-page PDF to insert from

    // A simple 3-page PDF (no display needed; runs under QT_QPA_PLATFORM=offscreen).
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

    // Effective /Rotate for a page in the file at `path`.
    int pageRotation(const QString& path, int index) {
        QPDF q;
        q.processFile(path.toLocal8Bit().constData());
        QPDFPageDocumentHelper dh(q);
        dh.pushInheritedAttributesToPage();
        auto pages = dh.getAllPages();
        QPDFObjectHandle rot = pages.at(index).getObjectHandle().getKey("/Rotate");
        return rot.isInteger() ? rot.getIntValueAsInt() : 0;
    }

    int pageCount(const QString& path) {
        QPDF q;
        q.processFile(path.toLocal8Bit().constData());
        return static_cast<int>(QPDFPageDocumentHelper(q).getAllPages().size());
    }

    // Read box `key` ("/MediaBox" or "/CropBox") of page `index`, normalized to
    // [x0,y0,x1,y1]. Returns false if the page has no such 4-element box.
    bool boxOf(const QString& path, int index, const char* key, double out[4]) {
        QPDF q;
        q.processFile(path.toLocal8Bit().constData());
        QPDFPageDocumentHelper dh(q);
        dh.pushInheritedAttributesToPage();
        auto pages = dh.getAllPages();
        QPDFObjectHandle b = pages.at(index).getObjectHandle().getKey(key);
        if (!b.isArray() || b.getArrayNItems() != 4)
            return false;
        double v[4];
        for (int i = 0; i < 4; ++i)
            v[i] = b.getArrayItem(i).getNumericValue();
        out[0] = qMin(v[0], v[2]);
        out[1] = qMin(v[1], v[3]);
        out[2] = qMax(v[0], v[2]);
        out[3] = qMax(v[1], v[3]);
        return true;
    }

    // A flattened view of a written outline for assertions.
    struct OL {
        QString title;
        int page;
        QVector<OL> kids;
    };
    QVector<OL> readOutline(const QString& path) {
        QPDF q;
        q.processFile(path.toLocal8Bit().constData());
        QPDFPageDocumentHelper pdh(q);
        std::vector<QPDFPageObjectHelper> pages = pdh.getAllPages();
        const auto destIndex = [&](QPDFOutlineObjectHelper& o) -> int {
            QPDFObjectHandle dest = o.getObjectHandle().getKey("/Dest");
            if (!dest.isArray() || dest.getArrayNItems() < 1)
                return -1;
            QPDFObjGen g = dest.getArrayItem(0).getObjGen();
            for (size_t i = 0; i < pages.size(); ++i)
                if (pages[i].getObjectHandle().getObjGen() == g)
                    return static_cast<int>(i);
            return -1;
        };
        std::function<QVector<OL>(std::vector<QPDFOutlineObjectHelper>)> conv =
            [&](std::vector<QPDFOutlineObjectHelper> items) {
                QVector<OL> out;
                for (auto& it : items)
                    out.push_back(OL{QString::fromStdString(it.getTitle()), destIndex(it),
                                     conv(it.getKids())});
                return out;
            };
        QPDFOutlineDocumentHelper odh(q);
        return conv(odh.getTopLevelOutlines());
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_input = m_dir.filePath("input.pdf");
        makeSample(m_input, 3);
        QCOMPARE(pageCount(m_input), 3);
        m_source = m_dir.filePath("source.pdf");
        makeSample(m_source, 2);
        QCOMPARE(pageCount(m_source), 2);
    }

    void identityKeepsAllPages() {
        const QString out = m_dir.filePath("identity.pdf");
        QString err;
        QVERIFY2(PdfEditor::saveArrangement(m_input, out, {0, 1, 2}, {0, 0, 0}, &err),
                 qPrintable(err));
        QCOMPARE(pageCount(out), 3);
    }

    void deleteAndReorder() {
        // Keep original pages 2 and 0 (drop page 1), reversed.
        const QString out = m_dir.filePath("reordered.pdf");
        QString err;
        QVERIFY2(PdfEditor::saveArrangement(m_input, out, {2, 0}, {0, 0}, &err), qPrintable(err));
        QCOMPARE(pageCount(out), 2);
    }

    void rotationIsWritten() {
        const QString out = m_dir.filePath("rotated.pdf");
        QString err;
        QVERIFY2(PdfEditor::saveArrangement(m_input, out, {0, 1, 2}, {90, 180, 0}, &err),
                 qPrintable(err));
        QCOMPARE(pageCount(out), 3);
        QCOMPARE(pageRotation(out, 0), 90);
        QCOMPARE(pageRotation(out, 1), 180);
        QCOMPARE(pageRotation(out, 2), 0);
    }

    void insertAtEndAppends() {
        const QString out = m_dir.filePath("ins-end.pdf");
        QString err;
        QVERIFY2(PdfEditor::insertPages(m_input, out, {0, 1, 2}, {0, 0, 0}, m_source, {0, 1},
                                        /*atSlot=*/3, &err),
                 qPrintable(err));
        QCOMPARE(pageCount(out), 5);
    }

    void insertAtBeginningPrepends() {
        // Base pages carry distinct rotations as markers; the inserted page has
        // none, so its position 0 and the preserved base order are both visible.
        const QString out = m_dir.filePath("ins-begin.pdf");
        QString err;
        QVERIFY2(PdfEditor::insertPages(m_input, out, {0, 1, 2}, {90, 180, 270}, m_source, {0},
                                        /*atSlot=*/0, &err),
                 qPrintable(err));
        QCOMPARE(pageCount(out), 4);
        QCOMPARE(pageRotation(out, 0), 0);   // inserted page, first
        QCOMPARE(pageRotation(out, 1), 90);  // base page 0
        QCOMPARE(pageRotation(out, 2), 180); // base page 1
        QCOMPARE(pageRotation(out, 3), 270); // base page 2
    }

    void insertInMiddleKeepsOrderAndRotation() {
        const QString out = m_dir.filePath("ins-mid.pdf");
        QString err;
        QVERIFY2(PdfEditor::insertPages(m_input, out, {0, 1, 2}, {90, 180, 270}, m_source, {0},
                                        /*atSlot=*/1, &err),
                 qPrintable(err));
        QCOMPARE(pageCount(out), 4);
        QCOMPARE(pageRotation(out, 0), 90);  // base page 0
        QCOMPARE(pageRotation(out, 1), 0);   // inserted page
        QCOMPARE(pageRotation(out, 2), 180); // base page 1
        QCOMPARE(pageRotation(out, 3), 270); // base page 2
    }

    void cropAllInsetsTheCropBox() {
        const QString out = m_dir.filePath("crop-all.pdf");
        QString err;
        // Distinct margins per edge (points), every page, no rotation.
        PdfEditor::CropMargins m{10, 20, 30, 40}; // left, right, top, bottom
        QVERIFY2(PdfEditor::cropPages(m_input, out, {0, 1, 2}, {0, 0, 0}, {}, m, &err),
                 qPrintable(err));
        for (int i = 0; i < 3; ++i) {
            double mb[4], cb[4];
            QVERIFY(boxOf(out, i, "/MediaBox", mb));
            QVERIFY(boxOf(out, i, "/CropBox", cb));
            QVERIFY(qAbs(cb[0] - (mb[0] + 10)) < 0.01); // left inset
            QVERIFY(qAbs(cb[1] - (mb[1] + 40)) < 0.01); // bottom inset
            QVERIFY(qAbs(cb[2] - (mb[2] - 20)) < 0.01); // right inset
            QVERIFY(qAbs(cb[3] - (mb[3] - 30)) < 0.01); // top inset
        }
    }

    void cropTargetsOnlySelectedSlots() {
        const QString out = m_dir.filePath("crop-one.pdf");
        QString err;
        PdfEditor::CropMargins m{10, 10, 10, 10};
        QVERIFY2(PdfEditor::cropPages(m_input, out, {0, 1, 2}, {0, 0, 0}, {1}, m, &err),
                 qPrintable(err));
        double cb[4];
        QVERIFY(!boxOf(out, 0, "/CropBox", cb)); // untouched
        QVERIFY(boxOf(out, 1, "/CropBox", cb));  // cropped
        QVERIFY(!boxOf(out, 2, "/CropBox", cb)); // untouched
    }

    void cropMapsMarginsThroughRotation() {
        const QString out = m_dir.filePath("crop-rot.pdf");
        QString err;
        // A displayed "left" margin on a page rotated 90° clockwise must trim the
        // page's unrotated *bottom* edge (and nothing else).
        PdfEditor::CropMargins m{10, 0, 0, 0}; // left only
        QVERIFY2(PdfEditor::cropPages(m_input, out, {0}, {90}, {0}, m, &err), qPrintable(err));
        QCOMPARE(pageRotation(out, 0), 90);
        double mb[4], cb[4];
        QVERIFY(boxOf(out, 0, "/MediaBox", mb));
        QVERIFY(boxOf(out, 0, "/CropBox", cb));
        QVERIFY(qAbs(cb[0] - mb[0]) < 0.01);        // left edge unchanged
        QVERIFY(qAbs(cb[1] - (mb[1] + 10)) < 0.01); // bottom inset by 10
        QVERIFY(qAbs(cb[2] - mb[2]) < 0.01);        // right edge unchanged
        QVERIFY(qAbs(cb[3] - mb[3]) < 0.01);        // top edge unchanged
    }

    void outlineWritesFlatBookmarks() {
        const QString out = m_dir.filePath("ol-flat.pdf");
        QString err;
        QVector<PdfEditor::OutlineItem> items{{"Intro", 0, {}}, {"Body", 1, {}}, {"End", 2, {}}};
        QVERIFY2(PdfEditor::setOutline(m_input, out, items, &err), qPrintable(err));
        const QVector<OL> got = readOutline(out);
        QCOMPARE(got.size(), 3);
        QCOMPARE(got[0].title, QStringLiteral("Intro"));
        QCOMPARE(got[0].page, 0);
        QCOMPARE(got[1].title, QStringLiteral("Body"));
        QCOMPARE(got[1].page, 1);
        QCOMPARE(got[2].title, QStringLiteral("End"));
        QCOMPARE(got[2].page, 2);
    }

    void outlineWritesNestedBookmarks() {
        const QString out = m_dir.filePath("ol-nested.pdf");
        QString err;
        QVector<PdfEditor::OutlineItem> items{
            {"Chapter 1", 0, {{"Section 1.1", 1, {}}, {"Section 1.2", 2, {}}}}};
        QVERIFY2(PdfEditor::setOutline(m_input, out, items, &err), qPrintable(err));
        const QVector<OL> got = readOutline(out);
        QCOMPARE(got.size(), 1);
        QCOMPARE(got[0].title, QStringLiteral("Chapter 1"));
        QCOMPARE(got[0].kids.size(), 2);
        QCOMPARE(got[0].kids[0].title, QStringLiteral("Section 1.1"));
        QCOMPARE(got[0].kids[0].page, 1);
        QCOMPARE(got[0].kids[1].page, 2);
    }

    void outlineEmptyRemovesIt() {
        const QString seeded = m_dir.filePath("ol-seeded.pdf");
        const QString cleared = m_dir.filePath("ol-cleared.pdf");
        QString err;
        QVERIFY2(PdfEditor::setOutline(m_input, seeded, {{"A", 0, {}}}, &err), qPrintable(err));
        QCOMPARE(readOutline(seeded).size(), 1);
        QVERIFY2(PdfEditor::setOutline(seeded, cleared, {}, &err), qPrintable(err));
        QCOMPARE(readOutline(cleared).size(), 0);
    }
};

QTEST_MAIN(TestPdfEditor)
#include "test_pdfeditor.moc"
