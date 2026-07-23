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

#include "ui/HeaderFooterDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

HeaderFooterDialog::HeaderFooterDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Header & Footer"));

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
                       "QLabel#Sec { color:%2; font-weight:600; }"
                       "QLabel { color:%2; }"
                       "QLineEdit, QSpinBox { background:%3; border:1px solid %4; border-radius:8px;"
                       " padding:5px 8px; color:%2; }"
                       "QLineEdit:focus, QSpinBox:focus { border:1px solid %5; }")
            .arg(css(p.dim), css(p.text), css(p.surface), css(ctlBorder), css(p.accent)));
    setStyleSheet(styleSheet() + Theme::instance().spinBoxArrowQss());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    auto* hint = new QLabel(
        tr("Tokens: {n} page number · {p} total pages · {date} today · {file} file name."), this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);
    root->addSpacing(4);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    grid->setColumnStretch(3, 1);

    auto colHead = [&](const QString& t, int col) {
        auto* l = new QLabel(t, this);
        grid->addWidget(l, 0, col, Qt::AlignHCenter);
    };
    colHead(tr("Left"), 1);
    colHead(tr("Centre"), 2);
    colHead(tr("Right"), 3);

    auto* hSec = new QLabel(tr("Header"), this);
    hSec->setObjectName(QStringLiteral("Sec"));
    grid->addWidget(hSec, 1, 0);
    m_hL = new QLineEdit(this);
    m_hC = new QLineEdit(this);
    m_hR = new QLineEdit(this);
    grid->addWidget(m_hL, 1, 1);
    grid->addWidget(m_hC, 1, 2);
    grid->addWidget(m_hR, 1, 3);

    auto* fSec = new QLabel(tr("Footer"), this);
    fSec->setObjectName(QStringLiteral("Sec"));
    grid->addWidget(fSec, 2, 0);
    m_fL = new QLineEdit(this);
    m_fC = new QLineEdit(this);
    m_fR = new QLineEdit(this);
    grid->addWidget(m_fL, 2, 1);
    grid->addWidget(m_fC, 2, 2);
    grid->addWidget(m_fR, 2, 3);
    root->addLayout(grid);

    // A common default: centred "Page X of Y".
    m_fC->setText(QStringLiteral("Page {n} of {p}"));

    root->addSpacing(6);
    auto* opts = new QGridLayout;
    opts->setHorizontalSpacing(8);
    opts->addWidget(new QLabel(tr("Font size:"), this), 0, 0);
    m_fontSize = new QSpinBox(this);
    m_fontSize->setRange(6, 48);
    m_fontSize->setValue(10);
    m_fontSize->setSuffix(tr(" pt"));
    opts->addWidget(m_fontSize, 0, 1);
    opts->addWidget(new QLabel(tr("Start {n} at:"), this), 0, 2);
    m_start = new QSpinBox(this);
    m_start->setRange(0, 1000000);
    m_start->setValue(1);
    opts->addWidget(m_start, 0, 3);
    opts->setColumnStretch(4, 1);
    root->addLayout(opts);

    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* run = buttons->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    run->setObjectName(QStringLiteral("Share"));
    run->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    resize(560, 0);
}

Watermarker::HeaderFooterOptions HeaderFooterDialog::options() const {
    Watermarker::HeaderFooterOptions o;
    o.headerLeft = m_hL->text();
    o.headerCenter = m_hC->text();
    o.headerRight = m_hR->text();
    o.footerLeft = m_fL->text();
    o.footerCenter = m_fC->text();
    o.footerRight = m_fR->text();
    o.fontSize = m_fontSize->value();
    o.startNumber = m_start->value();
    return o;
}

bool HeaderFooterDialog::anyText() const {
    return !(m_hL->text().isEmpty() && m_hC->text().isEmpty() && m_hR->text().isEmpty() &&
             m_fL->text().isEmpty() && m_fC->text().isEmpty() && m_fR->text().isEmpty());
}
