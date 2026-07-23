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

#include "ui/OptimizeDialog.h"

#include "ui/Theme.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

OptimizeDialog::OptimizeDialog(const Optimizer::Audit& audit, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Optimize Document"));

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
                       "QLabel#Section { color:%1; font-size:11px; font-weight:600; }"
                       "QLabel#Cat { color:%2; }"
                       "QLabel#CatBytes { color:%1; }"
                       "QCheckBox, QLabel { color:%2; }"
                       "QSpinBox { background:%3; border:1px solid %4; border-radius:8px;"
                       " padding:5px 8px; color:%2; }"
                       "QSpinBox:focus { border:1px solid %5; }"
                       "QSpinBox:disabled { color:%1; }"
                       "QProgressBar { background:%3; border:none; border-radius:3px; max-height:6px; }"
                       "QProgressBar::chunk { background:%5; border-radius:3px; }")
            .arg(css(p.dim), css(p.text), css(p.surface), css(ctlBorder), css(p.accent)));
    setStyleSheet(styleSheet() + Theme::instance().spinBoxArrowQss());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    auto* hint = new QLabel(tr("See what's taking up space, then choose how to shrink this file."),
                            this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);
    root->addSpacing(4);

    // --- Audit: bytes by category ---------------------------------------------
    auto* auditTitle = new QLabel(tr("WHAT'S TAKING SPACE"), this);
    auditTitle->setObjectName(QStringLiteral("Section"));
    root->addWidget(auditTitle);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(1, 1);
    const QLocale locale;
    const qint64 total = qMax<qint64>(audit.total, 1);
    int row = 0;
    const auto addCat = [&](const QString& name, qint64 bytes) {
        auto* label = new QLabel(name, this);
        label->setObjectName(QStringLiteral("Cat"));
        auto* bar = new QProgressBar(this);
        bar->setTextVisible(false);
        bar->setRange(0, 1000);
        bar->setValue(static_cast<int>(qBound<qint64>(0, bytes * 1000 / total, qint64(1000))));
        auto* size = new QLabel(locale.formattedDataSize(bytes), this);
        size->setObjectName(QStringLiteral("CatBytes"));
        size->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(label, row, 0);
        grid->addWidget(bar, row, 1);
        grid->addWidget(size, row, 2);
        ++row;
    };
    addCat(tr("Images"), audit.images);
    addCat(tr("Fonts"), audit.fonts);
    addCat(tr("Page content"), audit.content);
    addCat(tr("Metadata"), audit.metadata);
    addCat(tr("Other"), audit.other);
    root->addLayout(grid);

    auto* totalLabel =
        new QLabel(tr("Total: %1").arg(locale.formattedDataSize(audit.total)), this);
    totalLabel->setObjectName(QStringLiteral("CatBytes"));
    root->addWidget(totalLabel);
    root->addSpacing(8);

    // --- Options --------------------------------------------------------------
    auto* optTitle = new QLabel(tr("OPTIONS"), this);
    optTitle->setObjectName(QStringLiteral("Section"));
    root->addWidget(optTitle);

    auto* dpiRow = new QHBoxLayout;
    dpiRow->setSpacing(8);
    m_downsample = new QCheckBox(tr("Reduce image resolution to"), this);
    m_downsample->setChecked(audit.images > 0);
    m_dpi = new QSpinBox(this);
    m_dpi->setRange(36, 600);
    m_dpi->setValue(150);
    m_dpi->setSingleStep(25);
    m_dpi->setSuffix(tr(" DPI"));
    dpiRow->addWidget(m_downsample);
    dpiRow->addWidget(m_dpi);
    dpiRow->addStretch(1);
    root->addLayout(dpiRow);
    connect(m_downsample, &QCheckBox::toggled, m_dpi, &QWidget::setEnabled);
    m_dpi->setEnabled(m_downsample->isChecked());

    m_dropFonts = new QCheckBox(tr("Unembed fonts (remove embedded font programs)"), this);
    m_dropFonts->setChecked(false);
    root->addWidget(m_dropFonts);

    m_compress = new QCheckBox(tr("Recompress streams and pack objects"), this);
    m_compress->setChecked(true);
    root->addWidget(m_compress);

    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* run = buttons->addButton(tr("Optimize"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    run->setObjectName(QStringLiteral("Share")); // accent-filled primary
    run->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    resize(440, 0);
}

Optimizer::Options OptimizeDialog::options() const {
    Optimizer::Options o;
    o.targetDpi = m_downsample->isChecked() ? m_dpi->value() : 0;
    o.dropFonts = m_dropFonts->isChecked();
    o.compress = m_compress->isChecked();
    return o;
}
