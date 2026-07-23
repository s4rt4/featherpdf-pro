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

#include "ui/ProtectDialog.h"

#include "ui/Theme.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

ProtectDialog::ProtectDialog(const QString& docName, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Protect with Password"));

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
        QStringLiteral(
            "QLabel#Hint { color:%2; }"
            "QLabel#Status { color:%2; font-size:12px; }"
            "QLineEdit { background:%1; border:1px solid %3; border-radius:8px; padding:6px 9px;"
            " color:%4; selection-background-color:%5; selection-color:#FFFFFF; }"
            "QLineEdit:focus { border:1px solid %5; }"
            "QCheckBox { color:%4; spacing:9px; }"
            "QCheckBox::indicator { width:16px; height:16px; border:1px solid %3; border-radius:5px;"
            " background:%1; }"
            "QCheckBox::indicator:checked { border:1px solid %5; background:%5; }")
            .arg(css(p.surface), css(p.dim), css(ctlBorder), css(p.text), css(p.accent)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(14);

    auto* hint = new QLabel(
        tr("Protect “%1” with AES-256 encryption. Set a password to open it, restrict what "
           "recipients may do, or both. Keep the password safe - it cannot be recovered.")
            .arg(docName),
        this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* form = new QFormLayout;
    form->setSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("Leave empty for no open password"));
    m_confirm = new QLineEdit(this);
    m_confirm->setEchoMode(QLineEdit::Password);
    m_password->setMinimumWidth(260);
    // Qt doesn't fold stylesheet padding into a QLineEdit's size hint, so in a
    // packed form the fields compress and the (taller) placeholder text clips.
    // Pin a height that scales with the font to give the text room.
    const int fieldHeight = m_password->fontMetrics().height() + 14;
    m_password->setMinimumHeight(fieldHeight);
    m_confirm->setMinimumHeight(fieldHeight);
    form->addRow(tr("Open password"), m_password);
    form->addRow(tr("Confirm"), m_confirm);
    root->addLayout(form);

    auto* show = new QCheckBox(tr("Show password"), this);
    root->addWidget(show);

    // Permissions.
    auto* permHead = new QLabel(tr("ALLOW RECIPIENTS TO"), this);
    permHead->setObjectName(QStringLiteral("SectionHead"));
    root->addWidget(permHead);
    m_allowPrint = new QCheckBox(tr("Print the document"), this);
    m_allowCopy = new QCheckBox(tr("Copy text and graphics"), this);
    m_allowEdit = new QCheckBox(tr("Edit, annotate, and fill forms"), this);
    for (QCheckBox* c : {m_allowPrint, m_allowCopy, m_allowEdit}) {
        c->setChecked(true);
        root->addWidget(c);
    }

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("Status"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    m_protect = buttons->addButton(tr("Protect"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    m_protect->setObjectName(QStringLiteral("Share")); // accent-filled primary
    m_protect->setCursor(Qt::PointingHandCursor);
    root->addWidget(buttons);

    connect(show, &QCheckBox::toggled, this, [this](bool on) {
        const QLineEdit::EchoMode mode = on ? QLineEdit::Normal : QLineEdit::Password;
        m_password->setEchoMode(mode);
        m_confirm->setEchoMode(mode);
    });
    connect(m_password, &QLineEdit::textChanged, this, &ProtectDialog::validate);
    connect(m_confirm, &QLineEdit::textChanged, this, &ProtectDialog::validate);
    for (QCheckBox* c : {m_allowPrint, m_allowCopy, m_allowEdit})
        connect(c, &QCheckBox::toggled, this, &ProtectDialog::validate);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    validate();
    resize(440, 0);
}

void ProtectDialog::validate() {
    const QString pw = m_password->text();
    const QString cf = m_confirm->text();
    const bool restricted = !(allowPrinting() && allowCopying() && allowEditing());

    QString msg;
    if (!pw.isEmpty() && cf.isEmpty())
        msg = tr("Re-enter the password to confirm.");
    else if (!pw.isEmpty() && pw != cf)
        msg = tr("The passwords don’t match.");
    else if (pw.isEmpty() && !restricted)
        msg = tr("Set an open password or restrict at least one permission.");

    const bool passwordsOk = pw.isEmpty() || pw == cf;
    m_status->setText(msg);
    m_protect->setEnabled(passwordsOk && (!pw.isEmpty() || restricted));
}

QString ProtectDialog::password() const {
    return m_password->text();
}

bool ProtectDialog::allowPrinting() const {
    return m_allowPrint->isChecked();
}

bool ProtectDialog::allowCopying() const {
    return m_allowCopy->isChecked();
}

bool ProtectDialog::allowEditing() const {
    return m_allowEdit->isChecked();
}
