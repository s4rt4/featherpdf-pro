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

#include "ui/ExtractDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

ExtractDialog::ExtractDialog(int pageCount, int current, QWidget* parent)
    : QDialog(parent), m_pageCount(qMax(0, pageCount)) {
    setWindowTitle(tr("Extract Pages"));

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

    auto* hint = new QLabel(tr("Save these pages from the current document to a new PDF."), this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);
    root->addSpacing(4);

    root->addWidget(new QLabel(tr("Pages (1–%1):").arg(qMax(1, m_pageCount)), this));
    m_pages = new QLineEdit(this);
    m_pages->setPlaceholderText(tr("e.g. 1-3, 5, 8-10"));
    if (current >= 1 && current <= m_pageCount)
        m_pages->setText(QString::number(current));
    root->addWidget(m_pages);

    root->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* run = buttons->addButton(tr("Extract"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    run->setObjectName(QStringLiteral("Share")); // accent-filled primary
    run->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    resize(420, 0);
    m_pages->setFocus();
    m_pages->selectAll();
}

QVector<int> ExtractDialog::selectedDisplayIndices() const {
    // Parse "1-3, 5, 8-10" into 0-based display indices, preserving typed order
    // (mirrors the range syntax used by Split). Out-of-range numbers are skipped.
    QVector<int> out;
    for (QString part : m_pages->text().split(',', Qt::SkipEmptyParts)) {
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
