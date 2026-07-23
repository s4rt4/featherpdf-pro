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

#include "ui/SplitDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

SplitDialog::SplitDialog(int pageCount, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Split"));

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
                       "QRadioButton, QLabel { color:%2; }"
                       "QSpinBox, QLineEdit { background:%3; border:1px solid %4;"
                       " border-radius:8px; padding:5px 8px; color:%2; }"
                       "QSpinBox:focus, QLineEdit:focus { border:1px solid %5; }")
            .arg(css(p.dim), css(p.text), css(p.surface), css(ctlBorder), css(p.accent)));
    setStyleSheet(styleSheet() + Theme::instance().spinBoxArrowQss());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    auto* hint = new QLabel(tr("Split this %n-page document into several files.", "", pageCount),
                            this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);
    root->addSpacing(6);

    m_each = new QRadioButton(tr("One file per page"), this);
    m_each->setChecked(true);
    root->addWidget(m_each);

    auto* nRow = new QHBoxLayout;
    m_everyN = new QRadioButton(tr("Every"), this);
    m_n = new QSpinBox(this);
    m_n->setRange(1, std::max(1, pageCount));
    m_n->setValue(std::min(10, std::max(1, pageCount)));
    m_n->setFixedWidth(80);
    m_n->setEnabled(false);
    nRow->addWidget(m_everyN);
    nRow->addWidget(m_n);
    nRow->addWidget(new QLabel(tr("pages"), this));
    nRow->addStretch(1);
    root->addLayout(nRow);

    auto* rRow = new QHBoxLayout;
    m_ranges = new QRadioButton(tr("Ranges"), this);
    m_rangesText = new QLineEdit(this);
    m_rangesText->setPlaceholderText(tr("e.g. 1-3, 5, 8-10"));
    m_rangesText->setEnabled(false);
    rRow->addWidget(m_ranges);
    rRow->addWidget(m_rangesText, 1);
    root->addLayout(rRow);

    connect(m_everyN, &QRadioButton::toggled, m_n, &QWidget::setEnabled);
    connect(m_ranges, &QRadioButton::toggled, m_rangesText, &QWidget::setEnabled);

    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* run = buttons->addButton(tr("Split"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    run->setObjectName(QStringLiteral("Share"));
    run->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    resize(430, 0);
}

Splitter::Mode SplitDialog::mode() const {
    if (m_everyN->isChecked())
        return Splitter::Mode::EveryN;
    if (m_ranges->isChecked())
        return Splitter::Mode::Ranges;
    return Splitter::Mode::EachPage;
}
int SplitDialog::everyN() const { return m_n->value(); }
QString SplitDialog::ranges() const { return m_rangesText->text(); }
