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

#include "ui/FindRedactDialog.h"

#include "ui/Theme.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace {
// Built-in patterns. Kept deliberately broad - over-matching is safer than
// missing sensitive data, and the user reviews the marks before applying.
const QString kEmail = QStringLiteral("[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}");
const QString kPhone = QStringLiteral("\\+?\\d[\\d ().\\-]{7,}\\d");
const QString kId = QStringLiteral("\\b\\d{16}\\b"); // 16-digit ID (e.g. NIK)
const QString kCard = QStringLiteral("\\b\\d(?:[ \\-]?\\d){12,18}\\b");
}

FindRedactDialog::FindRedactDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Find & Redact"));

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
                       "QCheckBox, QLabel { color:%2; }"
                       "QLineEdit { background:%3; border:1px solid %4; border-radius:8px;"
                       " padding:6px 9px; color:%2; }"
                       "QLineEdit:focus { border:1px solid %5; }")
            .arg(css(p.dim), css(p.text), css(p.surface), css(ctlBorder), css(p.accent)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    auto* hint = new QLabel(
        tr("Find text matching these patterns and mark it for redaction. You can review every "
           "mark before applying."),
        this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);
    root->addSpacing(4);

    m_email = new QCheckBox(tr("Email addresses"), this);
    m_phone = new QCheckBox(tr("Phone numbers"), this);
    m_id = new QCheckBox(tr("ID numbers (16 digits, e.g. NIK)"), this);
    m_card = new QCheckBox(tr("Card numbers (13–19 digits)"), this);
    m_email->setChecked(true);
    root->addWidget(m_email);
    root->addWidget(m_phone);
    root->addWidget(m_id);
    root->addWidget(m_card);

    root->addSpacing(6);
    root->addWidget(new QLabel(tr("Custom (regular expression):"), this));
    m_custom = new QLineEdit(this);
    m_custom->setPlaceholderText(tr("optional, e.g. \\bSECRET-\\d+\\b"));
    root->addWidget(m_custom);

    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* run = buttons->addButton(tr("Find"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    run->setObjectName(QStringLiteral("Share")); // accent-filled primary
    run->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    resize(440, 0);
}

bool FindRedactDialog::hasPattern() const {
    if (m_email->isChecked() || m_phone->isChecked() || m_id->isChecked() || m_card->isChecked())
        return true;
    const QString custom = m_custom->text().trimmed();
    return !custom.isEmpty() && QRegularExpression(custom).isValid();
}

QRegularExpression FindRedactDialog::pattern() const {
    QStringList parts;
    if (m_email->isChecked())
        parts << kEmail;
    if (m_phone->isChecked())
        parts << kPhone;
    if (m_id->isChecked())
        parts << kId;
    if (m_card->isChecked())
        parts << kCard;
    const QString custom = m_custom->text().trimmed();
    if (!custom.isEmpty() && QRegularExpression(custom).isValid())
        parts << custom;

    QStringList wrapped;
    for (const QString& part : parts)
        wrapped << QStringLiteral("(?:%1)").arg(part);
    return QRegularExpression(wrapped.join(QLatin1Char('|')),
                              QRegularExpression::CaseInsensitiveOption);
}
