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

#include "ui/CropDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace {
constexpr double kPtPerMm = 72.0 / 25.4;
} // namespace

CropDialog::CropDialog(int pageCount, QWidget* parent)
    : QDialog(parent), m_pageCount(qMax(0, pageCount)) {
    setWindowTitle(tr("Crop Pages"));

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
                       "QDoubleSpinBox, QLineEdit { background:%3; border:1px solid %4;"
                       " border-radius:8px; padding:5px 8px; color:%2; }"
                       "QDoubleSpinBox:focus, QLineEdit:focus { border:1px solid %5; }")
            .arg(css(p.dim), css(p.text), css(p.surface), css(ctlBorder), css(p.accent)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    auto* hint = new QLabel(
        tr("Trim the margins off your pages. The content stays, only the visible crop changes."),
        this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);
    root->addSpacing(6);

    const auto makeSpin = [&] {
        auto* s = new QDoubleSpinBox(this);
        s->setRange(0, 2000);
        s->setDecimals(1);
        s->setSingleStep(1.0);
        s->setSuffix(tr(" mm"));
        s->setFixedWidth(110);
        return s;
    };
    m_top = makeSpin();
    m_bottom = makeSpin();
    m_left = makeSpin();
    m_right = makeSpin();

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(6);
    grid->addWidget(new QLabel(tr("Top"), this), 0, 0);
    grid->addWidget(m_top, 0, 1);
    grid->addWidget(new QLabel(tr("Bottom"), this), 0, 2);
    grid->addWidget(m_bottom, 0, 3);
    grid->addWidget(new QLabel(tr("Left"), this), 1, 0);
    grid->addWidget(m_left, 1, 1);
    grid->addWidget(new QLabel(tr("Right"), this), 1, 2);
    grid->addWidget(m_right, 1, 3);
    grid->setColumnStretch(4, 1);
    root->addLayout(grid);

    root->addSpacing(8);

    m_allPages = new QRadioButton(tr("All pages"), this);
    m_allPages->setChecked(true);
    root->addWidget(m_allPages);

    auto* someRow = new QHBoxLayout;
    m_somePages = new QRadioButton(tr("Pages"), this);
    m_rangeText = new QLineEdit(this);
    m_rangeText->setPlaceholderText(tr("e.g. 1-3, 5, 8-10"));
    m_rangeText->setEnabled(false);
    someRow->addWidget(m_somePages);
    someRow->addWidget(m_rangeText, 1);
    root->addLayout(someRow);
    connect(m_somePages, &QRadioButton::toggled, m_rangeText, &QWidget::setEnabled);

    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* run = buttons->addButton(tr("Crop"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    run->setObjectName(QStringLiteral("Share")); // accent-filled primary
    run->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    resize(440, 0);
}

double CropDialog::leftPt() const { return m_left->value() * kPtPerMm; }
double CropDialog::rightPt() const { return m_right->value() * kPtPerMm; }
double CropDialog::topPt() const { return m_top->value() * kPtPerMm; }
double CropDialog::bottomPt() const { return m_bottom->value() * kPtPerMm; }

bool CropDialog::isEmptyCrop() const {
    return m_top->value() == 0 && m_bottom->value() == 0 && m_left->value() == 0 &&
           m_right->value() == 0;
}

QVector<int> CropDialog::selectedSlots() const {
    if (m_allPages->isChecked())
        return {}; // empty = every page
    // Parse "1-3, 5, 8-10" into 0-based slots (mirrors Split / Extract syntax).
    QVector<int> out;
    for (QString part : m_rangeText->text().split(',', Qt::SkipEmptyParts)) {
        part = part.trimmed();
        if (part.contains('-')) {
            const QStringList e = part.split('-');
            if (e.size() != 2)
                continue;
            bool ok1 = false, ok2 = false;
            int a = e[0].trimmed().toInt(&ok1), b = e[1].trimmed().toInt(&ok2);
            if (!ok1 || !ok2)
                continue;
            if (a > b)
                std::swap(a, b);
            for (int n = a; n <= b; ++n)
                if (n >= 1 && n <= m_pageCount)
                    out.push_back(n - 1);
        } else {
            bool ok = false;
            const int n = part.toInt(&ok);
            if (ok && n >= 1 && n <= m_pageCount)
                out.push_back(n - 1);
        }
    }
    return out;
}
