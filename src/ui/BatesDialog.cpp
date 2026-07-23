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

#include "ui/BatesDialog.h"

#include "ui/Theme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

BatesDialog::BatesDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Bates Numbering"));

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
    const QString chev = Theme::instance().symbolicIconPath(QStringLiteral("chevron-down"), p.dim);
    const QString chevUp = Theme::instance().symbolicIconPath(QStringLiteral("chevron-up"), p.dim);
    setStyleSheet(
        QStringLiteral(
            "QLabel#Hint { color:%2; }"
            "QLineEdit, QSpinBox, QComboBox { background:%1; border:1px solid %3; border-radius:8px;"
            " padding:6px 9px; color:%4; selection-background-color:%5; selection-color:#FFFFFF; }"
            "QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border:1px solid %5; }"
            "QComboBox::drop-down { border:none; width:24px; }"
            "QComboBox::down-arrow { image:url(%6); width:12px; height:12px; }"
            "QComboBox QAbstractItemView { background:%1; border:1px solid %3; border-radius:8px;"
            " padding:4px; outline:0; selection-background-color:%8; selection-color:%4; }"
            "QSpinBox::up-button, QSpinBox::down-button { background:transparent; border:none;"
            " width:18px; }"
            "QSpinBox::up-arrow { image:url(%7); width:11px; height:11px; }"
            "QSpinBox::down-arrow { image:url(%6); width:11px; height:11px; }")
            .arg(css(p.surface), css(p.dim), css(ctlBorder), css(p.text), css(p.accent), chev,
                 chevUp, css(p.accentTint)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(14);

    auto* hint = new QLabel(
        tr("Stamp a sequential identifier on every page (e.g. ABC-000001)."), this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_prefix = new QLineEdit(QStringLiteral("ABC-"), this);
    m_prefix->setMinimumWidth(220);
    m_suffix = new QLineEdit(this);
    m_start = new QSpinBox(this);
    m_start->setRange(0, 1000000000);
    m_start->setValue(1);
    m_digits = new QSpinBox(this);
    m_digits->setRange(1, 10);
    m_digits->setValue(6);
    m_corner = new QComboBox(this);
    m_corner->addItem(tr("Bottom right"), int(Watermarker::Corner::BottomRight));
    m_corner->addItem(tr("Bottom left"), int(Watermarker::Corner::BottomLeft));
    m_corner->addItem(tr("Top right"), int(Watermarker::Corner::TopRight));
    m_corner->addItem(tr("Top left"), int(Watermarker::Corner::TopLeft));

    auto* form = new QFormLayout;
    form->setSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(tr("Prefix"), m_prefix);
    form->addRow(tr("Suffix"), m_suffix);
    form->addRow(tr("Start at"), m_start);
    form->addRow(tr("Digits"), m_digits);
    form->addRow(tr("Position"), m_corner);
    root->addLayout(form);
    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* apply = buttons->addButton(tr("Apply Numbering"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    apply->setObjectName(QStringLiteral("Share"));
    apply->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    resize(420, 0);
}

Watermarker::BatesOptions BatesDialog::options() const {
    Watermarker::BatesOptions o;
    o.prefix = m_prefix->text();
    o.suffix = m_suffix->text();
    o.start = m_start->value();
    o.digits = m_digits->value();
    o.corner = Watermarker::Corner(m_corner->currentData().toInt());
    return o;
}
