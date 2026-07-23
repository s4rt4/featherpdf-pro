// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// End-to-end test for form authoring: a field created with FormEditor (QPDF)
// must be readable — and fillable — through FormFiller (Poppler), which is the
// exact pipeline the Forms panel uses. This is the cross-library safety net.

#include "backends/FormEditor.h"
#include "backends/FormFiller.h"

#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRectF>
#include <QTemporaryDir>
#include <QtTest>

class TestFormRoundtrip : public QObject {
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

    // First field matching `name` (empty name → first field of `kind`).
    FormFiller::Field find(const QList<FormFiller::Field>& fields, const QString& name,
                           FormFiller::Kind kind) {
        for (const FormFiller::Field& f : fields)
            if ((name.isEmpty() || f.name == name) && f.kind == kind)
                return f;
        return FormFiller::Field{};
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_input = m_dir.filePath("input.pdf");
        makeSample(m_input, 1);
    }

    void textFieldReadableAndFillable() {
        const QString authored = m_dir.filePath("text.pdf");
        FormEditor::NewField f;
        f.type = FormEditor::Type::Text;
        f.name = "full_name";
        f.defaultValue = "hi";
        f.page = 0;
        f.rect = QRectF(0.1, 0.1, 0.4, 0.05);
        QString err;
        QVERIFY2(FormEditor::addField(m_input, authored, f, &err), qPrintable(err));

        // Poppler sees it as a text field with the right name and value.
        QList<FormFiller::Field> fields = FormFiller::read(authored);
        FormFiller::Field t = find(fields, "full_name", FormFiller::Kind::Text);
        QCOMPARE(t.name, QStringLiteral("full_name"));
        QCOMPARE(t.textValue, QStringLiteral("hi"));

        // And it can be filled and saved through the same Poppler path.
        const QString filled = m_dir.filePath("text-filled.pdf");
        QHash<int, QVariant> values;
        values.insert(t.id, QStringLiteral("world"));
        QVERIFY2(FormFiller::save(authored, filled, values, &err), qPrintable(err));
        FormFiller::Field after = find(FormFiller::read(filled), "full_name",
                                       FormFiller::Kind::Text);
        QCOMPARE(after.textValue, QStringLiteral("world"));
    }

    void checkBoxReadable() {
        const QString authored = m_dir.filePath("check.pdf");
        FormEditor::NewField f;
        f.type = FormEditor::Type::CheckBox;
        f.name = "agree";
        f.checked = true;
        f.page = 0;
        f.rect = QRectF(0.1, 0.1, 0.03, 0.02);
        QString err;
        QVERIFY2(FormEditor::addField(m_input, authored, f, &err), qPrintable(err));
        FormFiller::Field c = find(FormFiller::read(authored), "agree",
                                   FormFiller::Kind::CheckBox);
        QCOMPARE(c.name, QStringLiteral("agree"));
        QVERIFY(c.checked);
    }

    void dropdownReadable() {
        const QString authored = m_dir.filePath("drop.pdf");
        FormEditor::NewField f;
        f.type = FormEditor::Type::Dropdown;
        f.name = "color";
        f.options = {"Red", "Green", "Blue"};
        f.page = 0;
        f.rect = QRectF(0.1, 0.1, 0.3, 0.04);
        QString err;
        QVERIFY2(FormEditor::addField(m_input, authored, f, &err), qPrintable(err));
        FormFiller::Field c = find(FormFiller::read(authored), "color",
                                   FormFiller::Kind::ComboBox);
        QCOMPARE(c.name, QStringLiteral("color"));
        QVERIFY(c.options.contains(QStringLiteral("Green")));
        QCOMPARE(c.currentChoice, QStringLiteral("Red"));
    }

    void radioGroupReadable() {
        const QString authored = m_dir.filePath("radio.pdf");
        QString err;
        QVERIFY2(FormEditor::addRadioGroup(m_input, authored, "plan", {"Basic", "Pro", "Max"}, 0,
                                           QRectF(0.1, 0.1, 0.03, 0.02), &err),
                 qPrintable(err));
        // Poppler should expose a radio button carrying the group name.
        FormFiller::Field r = find(FormFiller::read(authored), QString(), FormFiller::Kind::Radio);
        QCOMPARE(r.kind, FormFiller::Kind::Radio);
        QVERIFY(r.name.contains(QStringLiteral("plan")));
    }
};

QTEST_MAIN(TestFormRoundtrip)
#include "test_formroundtrip.moc"
