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

#include "ui/PasswordDialog.h"

#include "ui/Theme.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

PasswordDialog::PasswordDialog(const QString& fileName, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Password Required"));

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
            "QLabel#Error { color:%6; font-size:12px; }"
            "QLineEdit { background:%1; border:1px solid %3; border-radius:8px; padding:6px 9px;"
            " color:%4; selection-background-color:%5; selection-color:#FFFFFF; }"
            "QLineEdit:focus { border:1px solid %5; }"
            "QCheckBox { color:%4; spacing:9px; }"
            "QCheckBox::indicator { width:16px; height:16px; border:1px solid %3; border-radius:5px;"
            " background:%1; }"
            "QCheckBox::indicator:checked { border:1px solid %5; background:%5; }")
            .arg(css(p.surface), css(p.dim), css(ctlBorder), css(p.text), css(p.accent),
                 css(p.destructive)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(12);

    auto* hint = new QLabel(
        tr("“%1” is protected. Enter its password to open it.").arg(fileName), this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("Password"));
    m_password->setMinimumWidth(280);
    // Qt leaves stylesheet padding out of the line edit's size hint, so the field
    // can shrink until the placeholder clips; pin a font-scaled height.
    m_password->setMinimumHeight(m_password->fontMetrics().height() + 14);
    root->addWidget(m_password);

    auto* show = new QCheckBox(tr("Show password"), this);
    root->addWidget(show);

    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("Error"));
    m_error->setWordWrap(true);
    m_error->hide();
    root->addWidget(m_error);

    auto* buttons = new QDialogButtonBox(this);
    m_open = buttons->addButton(tr("Open"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    m_open->setObjectName(QStringLiteral("Share")); // accent-filled primary
    m_open->setCursor(Qt::PointingHandCursor);
    m_open->setEnabled(false);
    root->addWidget(buttons);

    connect(show, &QCheckBox::toggled, this, [this](bool on) {
        m_password->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    connect(m_password, &QLineEdit::textChanged, this,
            [this](const QString& t) { m_open->setEnabled(!t.isEmpty()); });
    connect(m_password, &QLineEdit::returnPressed, m_open, [this] {
        if (m_open->isEnabled())
            accept();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    resize(380, 0);
}

QString PasswordDialog::password() const {
    return m_password->text();
}

void PasswordDialog::setError(const QString& message) {
    m_error->setText(message);
    m_error->setVisible(!message.isEmpty());
    m_password->clear();
    m_password->setFocus();
}
