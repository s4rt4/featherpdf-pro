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

#include "ui/SanitizeDialog.h"

#include "ui/Theme.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SanitizeDialog::SanitizeDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Remove Hidden Information"));

    const Theme::Palette& p = Theme::instance().palette();
    const auto css = [](const QColor& c) {
        return c.alpha() == 255 ? c.name(QColor::HexRgb)
                                : QStringLiteral("rgba(%1,%2,%3,%4)")
                                      .arg(c.red())
                                      .arg(c.green())
                                      .arg(c.blue())
                                      .arg(QString::number(c.alphaF(), 'f', 3));
    };
    setStyleSheet(QStringLiteral("QLabel#Hint { color:%1; font-size:12px; }"
                                 "QCheckBox, QLabel { color:%2; }")
                      .arg(css(p.dim), css(p.text)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    auto* hint = new QLabel(
        tr("Strip information that isn't visible on the page before you share this file."), this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);
    root->addSpacing(4);

    m_metadata = new QCheckBox(tr("Document metadata (author, title, XMP)"), this);
    m_attachments = new QCheckBox(tr("Embedded files and attachments"), this);
    m_scripts = new QCheckBox(tr("JavaScript and automatic actions"), this);
    m_metadata->setChecked(true);
    m_attachments->setChecked(true);
    m_scripts->setChecked(true);
    root->addWidget(m_metadata);
    root->addWidget(m_attachments);
    root->addWidget(m_scripts);

    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* run = buttons->addButton(tr("Clean"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    run->setObjectName(QStringLiteral("Share")); // accent-filled primary
    run->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    resize(420, 0);
}

Sanitizer::Options SanitizeDialog::options() const {
    Sanitizer::Options o;
    o.metadata = m_metadata->isChecked();
    o.attachments = m_attachments->isChecked();
    o.scripts = m_scripts->isChecked();
    return o;
}

bool SanitizeDialog::anyChecked() const {
    return m_metadata->isChecked() || m_attachments->isChecked() || m_scripts->isChecked();
}
