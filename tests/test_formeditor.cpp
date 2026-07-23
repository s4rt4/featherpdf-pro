// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Headless tests for the form-authoring backend (FormEditor / QPDF): a created
// field lands in the catalog's /AcroForm and on the page, with the right type.

#include "backends/FormEditor.h"

#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRectF>
#include <QTemporaryDir>
#include <QtTest>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

class TestFormEditor : public QObject {
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

    QPDFObjectHandle acroFields(QPDF& q) {
        return q.getRoot().getKey("/AcroForm").getKey("/Fields");
    }

    // Read box `key` of page `index` into [x0,y0,x1,y1]; false if absent.
    bool boxOf(const QString& path, int index, const char* key, double out[4]) {
        QPDF q;
        q.processFile(path.toLocal8Bit().constData());
        auto pages = QPDFPageDocumentHelper(q).getAllPages();
        QPDFObjectHandle b = pages.at(index).getObjectHandle().getKey(key);
        if (!b.isArray() || b.getArrayNItems() != 4)
            return false;
        for (int i = 0; i < 4; ++i)
            out[i] = b.getArrayItem(i).getNumericValue();
        return true;
    }

    // Copy `in` to `out` with page 0 marked /Rotate 90.
    void rotatePage0(const QString& in, const QString& out) {
        QPDF q;
        q.processFile(in.toLocal8Bit().constData());
        auto pages = QPDFPageDocumentHelper(q).getAllPages();
        pages.at(0).getObjectHandle().replaceKey("/Rotate", QPDFObjectHandle::newInteger(90));
        QPDFWriter w(q, out.toLocal8Bit().constData());
        w.write();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_input = m_dir.filePath("input.pdf");
        makeSample(m_input, 2);
    }

    void addsTextField() {
        const QString out = m_dir.filePath("text.pdf");
        FormEditor::NewField f;
        f.type = FormEditor::Type::Text;
        f.name = "full_name";
        f.defaultValue = "hi";
        f.page = 0;
        f.rect = QRectF(0.1, 0.1, 0.3, 0.04);
        QString err;
        QVERIFY2(FormEditor::addField(m_input, out, f, &err), qPrintable(err));

        QPDF q;
        q.processFile(out.toLocal8Bit().constData());
        QPDFObjectHandle acro = q.getRoot().getKey("/AcroForm");
        QVERIFY(acro.isDictionary());
        QVERIFY(acro.getKey("/NeedAppearances").isBool());
        QVERIFY(acro.getKey("/NeedAppearances").getBoolValue());

        QPDFObjectHandle fields = acroFields(q);
        QVERIFY(fields.isArray());
        QCOMPARE(fields.getArrayNItems(), 1);
        QPDFObjectHandle field = fields.getArrayItem(0);
        QCOMPARE(QString::fromStdString(field.getKey("/T").getUTF8Value()),
                 QStringLiteral("full_name"));
        QCOMPARE(QString::fromStdString(field.getKey("/FT").getName()), QStringLiteral("/Tx"));
        QCOMPARE(QString::fromStdString(field.getKey("/V").getUTF8Value()), QStringLiteral("hi"));

        // The widget is also on the page it targets.
        auto pages = QPDFPageDocumentHelper(q).getAllPages();
        QPDFObjectHandle annots = pages.at(0).getObjectHandle().getKey("/Annots");
        QVERIFY(annots.isArray());
        QVERIFY(annots.getArrayNItems() >= 1);
    }

    void addsCheckedCheckBox() {
        const QString out = m_dir.filePath("check.pdf");
        FormEditor::NewField f;
        f.type = FormEditor::Type::CheckBox;
        f.name = "agree";
        f.checked = true;
        f.page = 0;
        f.rect = QRectF(0.1, 0.1, 0.03, 0.02);
        QString err;
        QVERIFY2(FormEditor::addField(m_input, out, f, &err), qPrintable(err));

        QPDF q;
        q.processFile(out.toLocal8Bit().constData());
        QPDFObjectHandle field = acroFields(q).getArrayItem(0);
        QCOMPARE(QString::fromStdString(field.getKey("/FT").getName()), QStringLiteral("/Btn"));
        QCOMPARE(QString::fromStdString(field.getKey("/V").getName()), QStringLiteral("/Yes"));
        QCOMPARE(QString::fromStdString(field.getKey("/AS").getName()), QStringLiteral("/Yes"));
    }

    void addsDropdownWithOptions() {
        const QString out = m_dir.filePath("drop.pdf");
        FormEditor::NewField f;
        f.type = FormEditor::Type::Dropdown;
        f.name = "color";
        f.options = {"Red", "Green", "Blue"};
        f.page = 1;
        f.rect = QRectF(0.1, 0.1, 0.3, 0.04);
        QString err;
        QVERIFY2(FormEditor::addField(m_input, out, f, &err), qPrintable(err));

        QPDF q;
        q.processFile(out.toLocal8Bit().constData());
        QPDFObjectHandle field = acroFields(q).getArrayItem(0);
        QCOMPARE(QString::fromStdString(field.getKey("/FT").getName()), QStringLiteral("/Ch"));
        QVERIFY(field.getKey("/Ff").getIntValueAsInt() & (1 << 17)); // combo flag
        QPDFObjectHandle opt = field.getKey("/Opt");
        QVERIFY(opt.isArray());
        QCOMPARE(opt.getArrayNItems(), 3);
        QCOMPARE(QString::fromStdString(opt.getArrayItem(0).getUTF8Value()),
                 QStringLiteral("Red"));
        QCOMPARE(QString::fromStdString(field.getKey("/V").getUTF8Value()), QStringLiteral("Red"));
    }

    void accumulatesFields() {
        const QString one = m_dir.filePath("one.pdf");
        const QString two = m_dir.filePath("two.pdf");
        FormEditor::NewField a;
        a.type = FormEditor::Type::Text;
        a.name = "a";
        a.page = 0;
        a.rect = QRectF(0.1, 0.1, 0.3, 0.04);
        QString err;
        QVERIFY2(FormEditor::addField(m_input, one, a, &err), qPrintable(err));
        FormEditor::NewField b = a;
        b.name = "b";
        QVERIFY2(FormEditor::addField(one, two, b, &err), qPrintable(err));

        QPDF q;
        q.processFile(two.toLocal8Bit().constData());
        QCOMPARE(acroFields(q).getArrayNItems(), 2);
    }

    void addsRadioGroup() {
        const QString out = m_dir.filePath("radio.pdf");
        QString err;
        QVERIFY2(FormEditor::addRadioGroup(m_input, out, "plan", {"Basic", "Pro", "Max"}, 0,
                                           QRectF(0.1, 0.1, 0.03, 0.02), &err),
                 qPrintable(err));

        QPDF q;
        q.processFile(out.toLocal8Bit().constData());
        QPDFObjectHandle fields = acroFields(q);
        QCOMPARE(fields.getArrayNItems(), 1); // the parent only
        QPDFObjectHandle parent = fields.getArrayItem(0);
        QCOMPARE(QString::fromStdString(parent.getKey("/FT").getName()), QStringLiteral("/Btn"));
        QVERIFY(parent.getKey("/Ff").getIntValueAsInt() & (1 << 15)); // radio flag
        QPDFObjectHandle kids = parent.getKey("/Kids");
        QVERIFY(kids.isArray());
        QCOMPARE(kids.getArrayNItems(), 3);

        QPDFObjectHandle kid = kids.getArrayItem(0);
        QCOMPARE(QString::fromStdString(kid.getKey("/Subtype").getName()),
                 QStringLiteral("/Widget"));
        QVERIFY(kid.getKey("/Parent").getObjGen() == parent.getObjGen());
        QCOMPARE(QString::fromStdString(kid.getKey("/AS").getName()), QStringLiteral("/Off"));
        // /AP /N defines two states: the export value and /Off.
        QPDFObjectHandle apN = kid.getKey("/AP").getKey("/N");
        QVERIFY(apN.isDictionary());
        QCOMPARE(static_cast<int>(apN.getKeys().size()), 2);

        // All three buttons are on the page.
        auto pages = QPDFPageDocumentHelper(q).getAllPages();
        QCOMPARE(pages.at(0).getObjectHandle().getKey("/Annots").getArrayNItems(), 3);
    }

    void rejectsTooFewRadioOptions() {
        const QString out = m_dir.filePath("radio-bad.pdf");
        QString err;
        QVERIFY(!FormEditor::addRadioGroup(m_input, out, "x", {"only"}, 0,
                                           QRectF(0.1, 0.1, 0.03, 0.02), &err));
        QVERIFY(!err.isEmpty());
    }

    void generatesAppearances() {
        const QString out = m_dir.filePath("appear.pdf");
        FormEditor::NewField f;
        f.type = FormEditor::Type::Text;
        f.name = "ap";
        f.defaultValue = "x";
        f.page = 0;
        f.rect = QRectF(0.1, 0.1, 0.3, 0.04);
        QString err;
        QVERIFY2(FormEditor::addField(m_input, out, f, &err), qPrintable(err));
        QPDF q;
        q.processFile(out.toLocal8Bit().constData());
        QPDFObjectHandle ap = acroFields(q).getArrayItem(0).getKey("/AP");
        QVERIFY(ap.isDictionary());
        QVERIFY(ap.getKey("/N").isStream()); // a single appearance stream

        const QString out2 = m_dir.filePath("appear-check.pdf");
        FormEditor::NewField c;
        c.type = FormEditor::Type::CheckBox;
        c.name = "ac";
        c.page = 0;
        c.rect = QRectF(0.1, 0.1, 0.03, 0.02);
        QVERIFY2(FormEditor::addField(m_input, out2, c, &err), qPrintable(err));
        QPDF q2;
        q2.processFile(out2.toLocal8Bit().constData());
        QPDFObjectHandle apN = acroFields(q2).getArrayItem(0).getKey("/AP").getKey("/N");
        QVERIFY(apN.isDictionary());
        QCOMPARE(static_cast<int>(apN.getKeys().size()), 2); // /Yes + /Off
    }

    void mapsRectThroughPageRotation() {
        const QString rotated = m_dir.filePath("rot-input.pdf");
        rotatePage0(m_input, rotated);
        const QString out = m_dir.filePath("rot-field.pdf");
        FormEditor::NewField f;
        f.type = FormEditor::Type::Text;
        f.name = "r";
        f.page = 0;
        f.rect = QRectF(0.0, 0.0, 0.5, 0.5); // displayed top-left quadrant
        QString err;
        QVERIFY2(FormEditor::addField(rotated, out, f, &err), qPrintable(err));

        QPDF q;
        q.processFile(out.toLocal8Bit().constData());
        QPDFObjectHandle field = acroFields(q).getArrayItem(0);
        QPDFObjectHandle rect = field.getKey("/Rect");
        double mb[4];
        QVERIFY(boxOf(out, 0, "/MediaBox", mb));
        const double W = mb[2] - mb[0], H = mb[3] - mb[1];
        // For /Rotate 90 the displayed top-left quadrant maps to the page's
        // unrotated bottom-left quadrant: x in [0, W/2], y in [0, H/2].
        QVERIFY(qAbs(rect.getArrayItem(0).getNumericValue() - 0.0) < 1.0);
        QVERIFY(qAbs(rect.getArrayItem(1).getNumericValue() - 0.0) < 1.0);
        QVERIFY(qAbs(rect.getArrayItem(2).getNumericValue() - W / 2) < 1.0);
        QVERIFY(qAbs(rect.getArrayItem(3).getNumericValue() - H / 2) < 1.0);
    }

    void deletesField() {
        const QString seeded = m_dir.filePath("del-seed.pdf");
        const QString out = m_dir.filePath("del-out.pdf");
        FormEditor::NewField f;
        f.type = FormEditor::Type::Text;
        f.name = "gone";
        f.page = 0;
        f.rect = QRectF(0.1, 0.1, 0.3, 0.04);
        QString err;
        QVERIFY2(FormEditor::addField(m_input, seeded, f, &err), qPrintable(err));
        QVERIFY2(FormEditor::deleteField(seeded, out, "gone", &err), qPrintable(err));

        QPDF q;
        q.processFile(out.toLocal8Bit().constData());
        QCOMPARE(acroFields(q).getArrayNItems(), 0);
        QPDFObjectHandle annots =
            QPDFPageDocumentHelper(q).getAllPages().at(0).getObjectHandle().getKey("/Annots");
        QVERIFY(!annots.isArray() || annots.getArrayNItems() == 0);

        QVERIFY(!FormEditor::deleteField(out, m_dir.filePath("x.pdf"), "missing", &err));
    }

    void movesField() {
        const QString seeded = m_dir.filePath("mv-seed.pdf");
        const QString out = m_dir.filePath("mv-out.pdf");
        FormEditor::NewField f;
        f.type = FormEditor::Type::Text;
        f.name = "mv";
        f.page = 0;
        f.rect = QRectF(0.1, 0.1, 0.2, 0.04);
        QString err;
        QVERIFY2(FormEditor::addField(m_input, seeded, f, &err), qPrintable(err));
        QVERIFY2(FormEditor::setFieldRect(seeded, out, "mv", QRectF(0.5, 0.5, 0.3, 0.05), &err),
                 qPrintable(err));

        QPDF q;
        q.processFile(out.toLocal8Bit().constData());
        QPDFObjectHandle rect = acroFields(q).getArrayItem(0).getKey("/Rect");
        double mb[4];
        QVERIFY(boxOf(out, 0, "/MediaBox", mb));
        const double W = mb[2] - mb[0];
        // New left edge ~ 0.5 * page width (upright page, no rotation).
        QVERIFY(qAbs(rect.getArrayItem(0).getNumericValue() - 0.5 * W) < 1.5);
    }

    void rejectsEmptyName() {
        const QString out = m_dir.filePath("noname.pdf");
        FormEditor::NewField f;
        f.type = FormEditor::Type::Text;
        f.name = "   ";
        f.page = 0;
        f.rect = QRectF(0.1, 0.1, 0.3, 0.04);
        QString err;
        QVERIFY(!FormEditor::addField(m_input, out, f, &err));
        QVERIFY(!err.isEmpty());
    }

    void writesFormatActions() {
        // A Currency-formatted Text field carries the standard Acrobat /AA
        // format + keystroke JavaScript so conforming viewers validate input.
        const QString out = m_dir.filePath("currency.pdf");
        FormEditor::NewField f;
        f.type = FormEditor::Type::Text;
        f.name = "amount";
        f.page = 0;
        f.rect = QRectF(0.1, 0.1, 0.3, 0.04);
        f.format = FormEditor::Format::Currency;
        QString err;
        QVERIFY2(FormEditor::addField(m_input, out, f, &err), qPrintable(err));

        QPDF q;
        q.processFile(out.toLocal8Bit().constData());
        QPDFObjectHandle field = acroFields(q).getArrayItem(0);
        QPDFObjectHandle aa = field.getKey("/AA");
        QVERIFY(aa.isDictionary());
        QPDFObjectHandle fmt = aa.getKey("/F");
        QVERIFY(fmt.isDictionary());
        QCOMPARE(QString::fromStdString(fmt.getKey("/S").getName()), QStringLiteral("/JavaScript"));
        const QString js = QString::fromStdString(fmt.getKey("/JS").getUTF8Value());
        QVERIFY2(js.contains("AFNumber_Format"), qPrintable(js));
        QVERIFY2(js.contains("$"), qPrintable(js));
        QVERIFY(aa.getKey("/K").isDictionary()); // keystroke filter too

        // A plain (None) field has no /AA.
        const QString plain = m_dir.filePath("plain.pdf");
        FormEditor::NewField g = f;
        g.name = "plain";
        g.format = FormEditor::Format::None;
        QVERIFY2(FormEditor::addField(m_input, plain, g, &err), qPrintable(err));
        QPDF q2;
        q2.processFile(plain.toLocal8Bit().constData());
        QVERIFY(!acroFields(q2).getArrayItem(0).getKey("/AA").isDictionary());
    }

    void addsManyFieldsAtOnce() {
        // Batch add: several detected fields land in one write, radios skipped.
        const QString out = m_dir.filePath("batch.pdf");
        QList<FormEditor::NewField> fields;
        for (int i = 0; i < 3; ++i) {
            FormEditor::NewField f;
            f.type = FormEditor::Type::Text;
            f.name = QStringLiteral("f%1").arg(i);
            f.page = i % 2;
            f.rect = QRectF(0.1, 0.1 + i * 0.05, 0.3, 0.04);
            fields.append(f);
        }
        FormEditor::NewField radio; // should be skipped by addFields
        radio.type = FormEditor::Type::Radio;
        radio.name = "ignored";
        radio.page = 0;
        radio.rect = QRectF(0.1, 0.5, 0.03, 0.02);
        fields.append(radio);

        int count = 0;
        QString err;
        QVERIFY2(FormEditor::addFields(m_input, out, fields, &count, &err), qPrintable(err));
        QCOMPARE(count, 3);

        QPDF q;
        q.processFile(out.toLocal8Bit().constData());
        QCOMPARE(acroFields(q).getArrayNItems(), 3);
    }
};

QTEST_MAIN(TestFormEditor)
#include "test_formeditor.moc"
