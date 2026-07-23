// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Round-trip test for XFDF form-data interchange: export reflects a field's
// value, and import applies a value back (verified through FormFiller/Poppler).

#include "backends/FormEditor.h"
#include "backends/FormFiller.h"
#include "backends/Xfdf.h"

#include <QFile>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRectF>
#include <QTemporaryDir>
#include <QtTest>

class TestXfdf : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_form; // a one-text-field document

    void makeSample(const QString& path) {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        painter.drawText(100, 100, QStringLiteral("Page 1"));
        painter.end();
    }

    QString readAll(const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();
        return QString::fromUtf8(f.readAll());
    }

    FormFiller::Field findText(const QString& path, const QString& name) {
        for (const FormFiller::Field& f : FormFiller::read(path))
            if (f.name == name)
                return f;
        return FormFiller::Field{};
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        const QString base = m_dir.filePath("base.pdf");
        makeSample(base);
        m_form = m_dir.filePath("form.pdf");
        FormEditor::NewField f;
        f.type = FormEditor::Type::Text;
        f.name = "name";
        f.defaultValue = "hi";
        f.page = 0;
        f.rect = QRectF(0.1, 0.1, 0.4, 0.05);
        QString err;
        QVERIFY2(FormEditor::addField(base, m_form, f, &err), qPrintable(err));
    }

    void exportContainsFieldValue() {
        const QString xfdf = m_dir.filePath("out.xfdf");
        QString err;
        QVERIFY2(Xfdf::exportFields(m_form, xfdf, &err), qPrintable(err));
        const QString xml = readAll(xfdf);
        QVERIFY(xml.contains(QStringLiteral("name=\"name\"")));
        QVERIFY(xml.contains(QStringLiteral("<value>hi</value>")));
    }

    void importAppliesValue() {
        const QString xfdf = m_dir.filePath("in.xfdf");
        QFile f(xfdf);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("<?xml version=\"1.0\"?>\n"
                "<xfdf xmlns=\"http://ns.adobe.com/xfdf/\">\n"
                "  <fields><field name=\"name\"><value>world</value></field></fields>\n"
                "</xfdf>\n");
        f.close();

        const QString out = m_dir.filePath("filled.pdf");
        QString err;
        QVERIFY2(Xfdf::importFields(m_form, xfdf, out, &err), qPrintable(err));
        QCOMPARE(findText(out, "name").textValue, QStringLiteral("world"));
    }

    void importRejectsNonMatching() {
        const QString xfdf = m_dir.filePath("other.xfdf");
        QFile f(xfdf);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("<xfdf xmlns=\"http://ns.adobe.com/xfdf/\"><fields>"
                "<field name=\"nope\"><value>x</value></field></fields></xfdf>");
        f.close();
        const QString out = m_dir.filePath("nomatch.pdf");
        QString err;
        QVERIFY(!Xfdf::importFields(m_form, xfdf, out, &err));
        QVERIFY(!err.isEmpty());
    }
};

QTEST_MAIN(TestXfdf)
#include "test_xfdf.moc"
