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

#include "ui/RedactionBar.h"

#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

RedactionBar::RedactionBar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("RedactionBar"));

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(14, 7, 12, 7);
    row->setSpacing(10);

    auto* dot = new QLabel(this);
    dot->setObjectName(QStringLiteral("RedactDot"));
    dot->setFixedSize(9, 9);

    m_label = new QLabel(this);
    m_label->setObjectName(QStringLiteral("RedactLabel"));

    m_apply = new QPushButton(tr("Apply Redaction"), this);
    m_apply->setObjectName(QStringLiteral("Share")); // accent-filled primary
    m_apply->setCursor(Qt::PointingHandCursor);

    m_clear = new QPushButton(tr("Clear"), this);
    m_clear->setObjectName(QStringLiteral("GhostBtn"));
    m_clear->setCursor(Qt::PointingHandCursor);

    auto* done = new QPushButton(tr("Done"), this);
    done->setObjectName(QStringLiteral("GhostBtn"));
    done->setCursor(Qt::PointingHandCursor);

    row->addWidget(dot);
    row->addWidget(m_label);
    row->addStretch(1);
    row->addWidget(m_clear);
    row->addWidget(m_apply);
    row->addWidget(done);

    connect(m_apply, &QPushButton::clicked, this, &RedactionBar::applyRequested);
    connect(m_clear, &QPushButton::clicked, this, &RedactionBar::clearRequested);
    connect(done, &QPushButton::clicked, this, &RedactionBar::doneRequested);

    // Re-apply on every theme change so colours follow light/dark.
    auto applyTheme = [this] {
        const Theme::Palette& p = Theme::instance().palette();
        const auto css = [](const QColor& c) {
            return c.alpha() == 255 ? c.name(QColor::HexRgb)
                                    : QStringLiteral("rgba(%1,%2,%3,%4)")
                                          .arg(c.red())
                                          .arg(c.green())
                                          .arg(c.blue())
                                          .arg(QString::number(c.alphaF(), 'f', 3));
        };
        setStyleSheet(
            QStringLiteral("#RedactionBar { background:%1; border-bottom:1px solid %2; }"
                           "#RedactLabel { color:%3; }"
                           "#RedactDot { background:%4; border-radius:4px; }")
                .arg(css(p.surface), css(p.hairline), css(p.text), css(p.destructive)));
    };
    applyTheme();
    connect(&Theme::instance(), &Theme::changed, this, applyTheme);

    setCount(0);
}

void RedactionBar::setCount(int count) {
    m_label->setText(count == 0
                         ? tr("Redaction mode - drag over anything you want permanently removed.")
                         : tr("%n area(s) marked for removal. Applying flattens those pages.", "",
                              count));
    m_apply->setEnabled(count > 0);
    m_clear->setEnabled(count > 0);
}
