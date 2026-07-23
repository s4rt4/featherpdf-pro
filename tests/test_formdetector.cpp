// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Headless tests for the "Prepare Form" auto-detector (FormDetector / Poppler):
// underscore fill-in lines become named Text fields (with a format guessed from
// the label), and bracket markers become CheckBox fields.

#include "backends/FormDetector.h"

#include <QFont>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QtTest>

class TestFormDetector : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_form;

    // A flat one-page "form": two labelled fill-in lines and a checkbox row.
    void makeForm(const QString& path) {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setResolution(72); // device units == points, so geometry is simple
        QPainter p(&writer);
        QFont f = p.font();
        f.setPointSize(12);
        p.setFont(f);
        p.drawText(72, 120, QStringLiteral("Full Name: ____________________"));
        p.drawText(72, 160, QStringLiteral("Date of Birth: ____________"));
        p.drawText(72, 200, QStringLiteral("Amount Paid: ____________"));
        p.drawText(72, 240, QStringLiteral("[ ] Subscribe to newsletter"));
        p.end();
    }

    const FormEditor::NewField* findText(const QList<FormEditor::NewField>& v,
                                         const QString& nameContains) {
        for (const FormEditor::NewField& f : v)
            if (f.type == FormEditor::Type::Text &&
                f.name.contains(nameContains, Qt::CaseInsensitive))
                return &f;
        return nullptr;
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_form = m_dir.filePath("form.pdf");
        makeForm(m_form);
    }

    void detectsUnderscoreTextFields() {
        QString err;
        const QList<FormEditor::NewField> fields = FormDetector::detect(m_form, &err);
        QVERIFY2(!fields.isEmpty(), qPrintable(err));

        const FormEditor::NewField* name = findText(fields, QStringLiteral("Name"));
        QVERIFY2(name, "expected a Text field named after the 'Full Name' label");
        QCOMPARE(name->page, 0);
        // Sits within the page in normalized coordinates.
        QVERIFY(name->rect.left() >= 0.0 && name->rect.right() <= 1.0);
        QVERIFY(name->rect.top() >= 0.0 && name->rect.bottom() <= 1.0);
        QVERIFY(name->rect.width() > 0.05); // a real fill-in span, not a speck
    }

    void guessesFormatFromLabel() {
        const QList<FormEditor::NewField> fields = FormDetector::detect(m_form, nullptr);
        const FormEditor::NewField* dob = findText(fields, QStringLiteral("Birth"));
        QVERIFY(dob);
        QCOMPARE(dob->format, FormEditor::Format::Date);
        const FormEditor::NewField* amount = findText(fields, QStringLiteral("Amount"));
        QVERIFY(amount);
        QCOMPARE(amount->format, FormEditor::Format::Currency);
    }

    void detectsCheckBox() {
        const QList<FormEditor::NewField> fields = FormDetector::detect(m_form, nullptr);
        int checks = 0;
        const FormEditor::NewField* box = nullptr;
        for (const FormEditor::NewField& f : fields)
            if (f.type == FormEditor::Type::CheckBox) {
                ++checks;
                box = &f;
            }
        QCOMPARE(checks, 1);
        QVERIFY(box);
        // Named after the trailing label, roughly square.
        QVERIFY2(box->name.contains(QStringLiteral("Subscribe"), Qt::CaseInsensitive),
                 qPrintable(box->name));
    }

    void namesAreUnique() {
        const QList<FormEditor::NewField> fields = FormDetector::detect(m_form, nullptr);
        QSet<QString> seen;
        for (const FormEditor::NewField& f : fields) {
            QVERIFY2(!seen.contains(f.name), qPrintable(f.name));
            seen.insert(f.name);
        }
    }

    void cleanDocumentYieldsNothing() {
        const QString plain = m_dir.filePath("plain.pdf");
        QPdfWriter writer(plain);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter p(&writer);
        p.drawText(100, 100, QStringLiteral("Just a paragraph of ordinary prose."));
        p.end();
        const QList<FormEditor::NewField> fields = FormDetector::detect(plain, nullptr);
        QVERIFY(fields.isEmpty());
    }
};

QTEST_MAIN(TestFormDetector)
#include "test_formdetector.moc"
