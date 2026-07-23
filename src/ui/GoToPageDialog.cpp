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

#include "ui/GoToPageDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

GoToPageDialog::GoToPageDialog(int pageCount, int current, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Go to Page"));

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
    const QString chevUp = Theme::instance().symbolicIconPath(QStringLiteral("chevron-up"), p.dim);
    const QString chevDown =
        Theme::instance().symbolicIconPath(QStringLiteral("chevron-down"), p.dim);
    setStyleSheet(
        QStringLiteral(
            "QLabel#Hint { color:%2; }"
            "QSpinBox { background:%1; border:1px solid %3; border-radius:8px; padding:6px 9px;"
            " color:%4; selection-background-color:%5; selection-color:#FFFFFF; min-height:18px; }"
            "QSpinBox:focus { border:1px solid %5; }"
            "QSpinBox::up-button, QSpinBox::down-button { background:transparent; border:none;"
            " width:20px; }"
            "QSpinBox::up-arrow { image:url(%6); width:11px; height:11px; }"
            "QSpinBox::down-arrow { image:url(%7); width:11px; height:11px; }")
            .arg(css(p.surface), css(p.dim), css(ctlBorder), css(p.text), css(p.accent), chevUp,
                 chevDown));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(12);

    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    auto* hint = new QLabel(tr("Page (1–%1):").arg(pageCount), this);
    hint->setObjectName(QStringLiteral("Hint"));
    row->addWidget(hint);
    m_spin = new QSpinBox(this);
    m_spin->setRange(1, qMax(1, pageCount));
    m_spin->setValue(qBound(1, current, qMax(1, pageCount)));
    m_spin->setMinimumWidth(90);
    m_spin->selectAll();
    row->addStretch(1);
    row->addWidget(m_spin);
    root->addLayout(row);

    auto* buttons = new QDialogButtonBox(this);
    auto* go = buttons->addButton(tr("Go"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    go->setObjectName(QStringLiteral("Share")); // accent-filled primary
    go->setCursor(Qt::PointingHandCursor);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_spin->setFocus();
}

int GoToPageDialog::selectedPage() const {
    return m_spin->value();
}
