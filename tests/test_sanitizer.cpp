// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Tests for Sanitize / Remove Hidden Information: a PDF salted with metadata,
// an embedded file, and scripts must come out clean, and the report must count
// what was stripped.

#include "backends/Sanitizer.h"

#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFWriter.hh>

class TestSanitizer : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_dirty;

    void makeBase(const QString& path) {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        painter.drawText(200, 300, QStringLiteral("Visible content"));
        painter.end();
    }

    // Salt a clean PDF with everything the sanitizer should remove.
    void makeDirty(const QString& base, const QString& dirty) {
        QPDF q;
        q.processFile(base.toLocal8Bit().constData());
        QPDFObjectHandle root = q.getRoot();

        QPDFObjectHandle info = q.makeIndirectObject(QPDFObjectHandle::newDictionary());
        info.replaceKey("/Author", QPDFObjectHandle::newString("Secret Author"));
        q.getTrailer().replaceKey("/Info", info);

        root.replaceKey("/Metadata", QPDFObjectHandle::newStream(&q, "<?xpacket?>xmp data"));

        QPDFObjectHandle names = QPDFObjectHandle::newDictionary();

        QPDFObjectHandle jsAction = QPDFObjectHandle::newDictionary();
        jsAction.replaceKey("/S", QPDFObjectHandle::newName("/JavaScript"));
        jsAction.replaceKey("/JS", QPDFObjectHandle::newString("app.alert(1)"));
        QPDFObjectHandle jsNames = QPDFObjectHandle::newArray();
        jsNames.appendItem(QPDFObjectHandle::newString("doc"));
        jsNames.appendItem(jsAction);
        QPDFObjectHandle js = QPDFObjectHandle::newDictionary();
        js.replaceKey("/Names", jsNames);
        names.replaceKey("/JavaScript", js);

        QPDFObjectHandle efNames = QPDFObjectHandle::newArray();
        efNames.appendItem(QPDFObjectHandle::newString("notes.txt"));
        efNames.appendItem(QPDFObjectHandle::newDictionary());
        QPDFObjectHandle ef = QPDFObjectHandle::newDictionary();
        ef.replaceKey("/Names", efNames);
        names.replaceKey("/EmbeddedFiles", ef);

        root.replaceKey("/Names", names);

        QPDFObjectHandle oa = QPDFObjectHandle::newDictionary();
        oa.replaceKey("/S", QPDFObjectHandle::newName("/JavaScript"));
        oa.replaceKey("/JS", QPDFObjectHandle::newString("app.alert(2)"));
        root.replaceKey("/OpenAction", oa);

        root.replaceKey("/AA", QPDFObjectHandle::newDictionary());

        QPDFWriter w(q, dirty.toLocal8Bit().constData());
        w.write();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        const QString base = m_dir.filePath("base.pdf");
        makeBase(base);
        m_dirty = m_dir.filePath("dirty.pdf");
        makeDirty(base, m_dirty);
    }

    void removesEverything() {
        const QString out = m_dir.filePath("clean.pdf");
        Sanitizer::Report rep;
        QString err;
        QVERIFY2(Sanitizer::sanitize(m_dirty, out, Sanitizer::Options{}, &rep, &err),
                 qPrintable(err));

        QCOMPARE(rep.metadata, 2);    // Info + XMP
        QCOMPARE(rep.attachments, 1); // one embedded file
        QVERIFY2(rep.scripts >= 3, "expected JS names + OpenAction + AA");

        QPDF q;
        q.processFile(out.toLocal8Bit().constData());
        QVERIFY(!q.getTrailer().hasKey("/Info"));
        QPDFObjectHandle root = q.getRoot();
        QVERIFY(!root.hasKey("/Metadata"));
        QVERIFY(!root.hasKey("/OpenAction"));
        QVERIFY(!root.hasKey("/AA"));
        // /Names is dropped once both subtrees are gone.
        QVERIFY(!root.hasKey("/Names"));
    }

    void respectsSelection() {
        const QString out = m_dir.filePath("meta-only.pdf");
        Sanitizer::Options opt;
        opt.attachments = false;
        opt.scripts = false;
        Sanitizer::Report rep;
        QString err;
        QVERIFY2(Sanitizer::sanitize(m_dirty, out, opt, &rep, &err), qPrintable(err));
        QCOMPARE(rep.metadata, 2);
        QCOMPARE(rep.attachments, 0);
        QCOMPARE(rep.scripts, 0);

        QPDF q;
        q.processFile(out.toLocal8Bit().constData());
        QVERIFY(!q.getTrailer().hasKey("/Info"));
        // Scripts were not selected, so the name tree (with /JavaScript) survives.
        QVERIFY(q.getRoot().hasKey("/Names"));
        QVERIFY(q.getRoot().hasKey("/OpenAction"));
    }
};

QTEST_MAIN(TestSanitizer)
#include "test_sanitizer.moc"
