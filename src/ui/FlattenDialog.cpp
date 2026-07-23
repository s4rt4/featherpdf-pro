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

#include "ui/FlattenDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

FlattenDialog::FlattenDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Flatten"));

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
    setStyleSheet(QStringLiteral("QLabel#Hint { color:%1; font-size:12px; }"
                                 "QRadioButton, QLabel { color:%2; }"
                                 "QSpinBox { background:%3; border:1px solid %4; border-radius:8px;"
                                 " padding:5px 8px; color:%2; }")
                      .arg(css(p.dim), css(p.text), css(p.surface), css(ctlBorder)));
    setStyleSheet(styleSheet() + Theme::instance().spinBoxArrowQss());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    auto* hint = new QLabel(tr("Make annotations and form fields non-interactive."), this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);
    root->addSpacing(6);

    m_lossless = new QRadioButton(tr("Lossless - bake in, keep selectable text"), this);
    m_lossless->setChecked(true);
    root->addWidget(m_lossless);
    auto* l1 = new QLabel(tr("Annotations and filled fields become part of the page; text stays "
                             "searchable."),
                          this);
    l1->setObjectName(QStringLiteral("Hint"));
    l1->setWordWrap(true);
    l1->setContentsMargins(26, 0, 0, 6);
    root->addWidget(l1);

    m_raster = new QRadioButton(tr("Raster - render every page to an image"), this);
    root->addWidget(m_raster);
    auto* l2 = new QLabel(tr("Guarantees a flat result but loses selectable text."), this);
    l2->setObjectName(QStringLiteral("Hint"));
    l2->setWordWrap(true);
    l2->setContentsMargins(26, 0, 0, 0);
    root->addWidget(l2);

    auto* dpiRow = new QVBoxLayout;
    dpiRow->setContentsMargins(26, 6, 0, 0);
    m_dpi = new QSpinBox(this);
    m_dpi->setRange(72, 600);
    m_dpi->setValue(200);
    m_dpi->setSuffix(QStringLiteral(" dpi"));
    m_dpi->setEnabled(false);
    m_dpi->setFixedWidth(120);
    dpiRow->addWidget(m_dpi);
    root->addLayout(dpiRow);
    connect(m_raster, &QRadioButton::toggled, m_dpi, &QWidget::setEnabled);

    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* run = buttons->addButton(tr("Flatten"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    run->setObjectName(QStringLiteral("Share"));
    run->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    resize(440, 0);
}

bool FlattenDialog::raster() const { return m_raster->isChecked(); }
int FlattenDialog::dpi() const { return m_dpi->value(); }
