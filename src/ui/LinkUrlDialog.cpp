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

#include "ui/LinkUrlDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

LinkUrlDialog::LinkUrlDialog(const QString& initial, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Link URL"));

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
                       "QLabel { color:%2; }"
                       "QLineEdit { background:%3; border:1px solid %4; border-radius:8px;"
                       " padding:6px 9px; color:%2; }"
                       "QLineEdit:focus { border:1px solid %5; }")
            .arg(css(p.dim), css(p.text), css(p.surface), css(ctlBorder), css(p.accent)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    auto* hint = new QLabel(tr("Where should this link go?"), this);
    hint->setObjectName(QStringLiteral("Hint"));
    root->addWidget(hint);

    m_url = new QLineEdit(this);
    m_url->setText(initial);
    m_url->setPlaceholderText(tr("https://example.com"));
    root->addWidget(m_url);

    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* ok = buttons->addButton(tr("OK"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    ok->setObjectName(QStringLiteral("Share"));
    ok->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    resize(420, 0);
    m_url->setFocus();
    m_url->selectAll();
}

QString LinkUrlDialog::url() const { return m_url->text().trimmed(); }
