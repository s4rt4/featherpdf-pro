// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "ui/PdfADialog.h"

#include "backends/PdfA.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

PdfADialog::PdfADialog(const QString& inputPath, QWidget* parent)
    : QDialog(parent), m_inputPath(inputPath), m_lastOutput(inputPath) {
    setWindowTitle(tr("PDF/A & Preflight"));

    const Theme::Palette& p = Theme::instance().palette();
    QColor ctlBorder = p.text;
    ctlBorder.setAlpha(46);
    const auto css = [](const QColor& c) {
        return c.alpha() == 255 ? c.name(QColor::HexRgb)
                                : QStringLiteral("rgba(%1,%2,%3,%4)")
                                      .arg(c.red())
                                      .arg(c.green())
                                      .arg(c.blue())
                                      .arg(QString::number(c.alphaF(), 'f', 3));
    };
    setStyleSheet(
        QStringLiteral("QLabel#Hint { color:%1; font-size:12px; }"
                       "QLabel#Status { color:%2; font-size:12px; font-weight:600; }"
                       "QLabel { color:%2; }"
                       "QPlainTextEdit { background:%3; border:1px solid %4; border-radius:8px;"
                       " padding:6px 8px; color:%2; }"
                       "QPlainTextEdit:focus { border:1px solid %5; }")
            .arg(css(p.dim), css(p.text), css(p.surface), css(ctlBorder), css(p.accent)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    auto* hint = new QLabel(
        tr("Move this file toward the PDF/A-1b archival standard, then check it with a "
           "preflight validator. Tagging is best-effort and won't fix non-conforming page "
           "content."),
        this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);
    root->addSpacing(4);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("Status"));
    m_status->setWordWrap(true);
    if (!PdfA::hasValidator())
        m_status->setText(tr("veraPDF isn't installed, tagging works, preflight won't."));
    root->addWidget(m_status);

    m_report = new QPlainTextEdit(this);
    m_report->setReadOnly(true);
    m_report->setMinimumHeight(160);
    m_report->setPlaceholderText(
        tr("Convert to tag the file as PDF/A-1b, or run a preflight to see the report here."));
    root->addWidget(m_report, 1);

    auto* buttons = new QDialogButtonBox(this);
    auto* validate = buttons->addButton(tr("Validate (preflight)"), QDialogButtonBox::ActionRole);
    auto* convert = buttons->addButton(tr("Convert to PDF/A-1b"), QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    convert->setObjectName(QStringLiteral("Share")); // accent-filled primary
    convert->setCursor(Qt::PointingHandCursor);
    validate->setCursor(Qt::PointingHandCursor);
    connect(convert, &QPushButton::clicked, this, &PdfADialog::doConvert);
    connect(validate, &QPushButton::clicked, this, &PdfADialog::doValidate);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    resize(460, 0);
}

void PdfADialog::report(const QString& text) {
    m_report->setPlainText(text);
}

void PdfADialog::doConvert() {
    const QFileInfo info(m_inputPath);
    const QString suggested =
        info.dir().filePath(info.completeBaseName() + QStringLiteral("-pdfa.pdf"));
    const QString out = QFileDialog::getSaveFileName(this, tr("Save PDF/A copy"), suggested,
                                                     tr("PDF documents (*.pdf)"));
    if (out.isEmpty())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = PdfA::convertToPdfA1b(m_inputPath, out, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        m_status->setText(tr("Conversion failed."));
        report(error);
        return;
    }
    m_lastOutput = out; // a later Validate checks the copy we just made
    m_status->setText(tr("Saved PDF/A-1b copy: %1").arg(QFileInfo(out).fileName()));
    report(tr("Wrote a copy tagged toward PDF/A-1b:\n  %1\n\n"
              "• Embedded an sRGB OutputIntent (GTS_PDFA1)\n"
              "• Wrote an XMP packet with pdfaid:part=1, conformance=B\n"
              "• Set /MarkInfo and a file /ID\n\n"
              "Run Validate (preflight) to check conformance.")
               .arg(out));
}

void PdfADialog::doValidate() {
    if (!PdfA::hasValidator()) {
        m_status->setText(tr("veraPDF isn't installed."));
        report(tr("Preflight needs the veraPDF validator (https://verapdf.org).\n"
                  "Install it and put 'verapdf' on your PATH, then try again."));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    PdfA::PreflightReport rep;
    QString error;
    const bool ok = PdfA::validate(m_lastOutput, &rep, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        m_status->setText(tr("Preflight couldn't run."));
        report(error);
        return;
    }
    if (!rep.available) {
        m_status->setText(tr("veraPDF isn't installed."));
        report(rep.summary);
        return;
    }

    m_status->setText(rep.pass ? tr("PASS: valid PDF/A-1b") : tr("FAIL: not valid PDF/A-1b"));
    QString body = tr("Checked: %1\nProfile: %2\nResult: %3\n")
                       .arg(QFileInfo(m_lastOutput).fileName(), rep.profile,
                            rep.pass ? tr("PASS") : tr("FAIL"));
    if (!rep.violations.isEmpty()) {
        body += QLatin1Char('\n') + tr("Violations:") + QLatin1Char('\n');
        for (const QString& v : rep.violations)
            body += QStringLiteral("  • %1\n").arg(v);
    } else if (!rep.pass) {
        body += QLatin1Char('\n') + rep.summary;
    }
    report(body);
}
