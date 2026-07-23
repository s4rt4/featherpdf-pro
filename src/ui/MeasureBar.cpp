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

#include "ui/MeasureBar.h"

#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolTip>

MeasureBar::MeasureBar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("MeasureBar"));

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(10, 6, 10, 6);
    row->setSpacing(6);

    // Icon-only mode buttons; the tooltip names each. Order defines modeChanged().
    const struct {
        const char* name;
        const char* icon;
    } modeDefs[] = {
        {QT_TR_NOOP("Distance"), "ruler"},
        {QT_TR_NOOP("Perimeter"), "line"},
        {QT_TR_NOOP("Area"), "square"},
    };
    for (const auto& d : modeDefs) {
        auto* b = new QPushButton(this);
        b->setObjectName(QStringLiteral("MeasureTool"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tr(d.name));
        b->setAccessibleName(tr(d.name));
        b->setFixedSize(34, 34);
        b->setIconSize(QSize(18, 18));
        m_modes.append(b);
        m_modeIcons << QLatin1String(d.icon);
    }
    m_modes.first()->setChecked(true);

    // Text unit buttons (mm / cm / in).
    const char* unitDefs[] = {QT_TR_NOOP("mm"), QT_TR_NOOP("cm"), QT_TR_NOOP("in")};
    for (const char* u : unitDefs) {
        auto* b = new QPushButton(tr(u), this);
        b->setObjectName(QStringLiteral("MeasureUnit"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(34);
        m_units.append(b);
    }
    m_units.first()->setChecked(true);

    // Usage hint folded into a hoverable "i" button so it never crowds the bar.
    m_info = new QPushButton(this);
    m_info->setObjectName(QStringLiteral("MeasureTool"));
    m_info->setCursor(Qt::WhatsThisCursor);
    m_info->setFixedSize(34, 34);
    m_info->setIconSize(QSize(18, 18));
    m_info->setFocusPolicy(Qt::NoFocus);
    const QString hint = tr("Click to add points. For perimeter and area, double-click or press "
                            "Esc to finish the shape.");
    m_info->setToolTip(hint);
    m_info->setAccessibleName(hint);
    connect(m_info, &QPushButton::clicked, this, [this, hint] {
        QToolTip::showText(m_info->mapToGlobal(QPoint(0, m_info->height())), hint, m_info);
    });

    m_clear = new QPushButton(tr("Clear"), this);
    m_clear->setObjectName(QStringLiteral("GhostBtn"));
    m_clear->setCursor(Qt::PointingHandCursor);

    auto* done = new QPushButton(tr("Done"), this);
    done->setObjectName(QStringLiteral("GhostBtn"));
    done->setCursor(Qt::PointingHandCursor);

    for (QPushButton* b : m_modes)
        row->addWidget(b);
    row->addSpacing(8);
    for (QPushButton* b : m_units)
        row->addWidget(b);
    row->addSpacing(6);
    row->addWidget(m_info);
    row->addStretch(1);
    row->addWidget(m_clear);
    row->addWidget(done);

    connect(m_clear, &QPushButton::clicked, this, &MeasureBar::clearRequested);
    connect(done, &QPushButton::clicked, this, &MeasureBar::doneRequested);
    for (int i = 0; i < m_modes.size(); ++i)
        connect(m_modes[i], &QPushButton::clicked, this, [this, i] {
            for (int j = 0; j < m_modes.size(); ++j)
                m_modes[j]->setChecked(j == i);
            applyIcons();
            emit modeChanged(i);
        });
    for (int i = 0; i < m_units.size(); ++i)
        connect(m_units[i], &QPushButton::clicked, this, [this, i] {
            for (int j = 0; j < m_units.size(); ++j)
                m_units[j]->setChecked(j == i);
            emit unitChanged(i);
        });

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
            QStringLiteral("#MeasureBar { background:%1; border-bottom:1px solid %2; }"
                           "#MeasureBar QPushButton#MeasureTool { background:transparent;"
                           " border:1px solid %2; border-radius:8px; padding:0; }"
                           "#MeasureBar QPushButton#MeasureTool:hover { background:%4; }"
                           "#MeasureBar QPushButton#MeasureTool:checked { background:%4;"
                           " border:1px solid %5; }"
                           "#MeasureBar QPushButton#MeasureUnit { background:transparent; color:%3;"
                           " border:1px solid %2; border-radius:8px; padding:0 10px; }"
                           "#MeasureBar QPushButton#MeasureUnit:hover { background:%4; }"
                           "#MeasureBar QPushButton#MeasureUnit:checked { background:%4; color:%5;"
                           " border:1px solid %5; }")
                .arg(css(p.surface), css(p.hairline), css(p.text), css(p.accentTint),
                     css(p.accent)));
        m_info->setIcon(Theme::instance().icon(QStringLiteral("info"), p.text));
        applyIcons();
    };
    applyTheme();
    connect(&Theme::instance(), &Theme::changed, this, applyTheme);
}

void MeasureBar::applyIcons() {
    const Theme::Palette& p = Theme::instance().palette();
    for (int i = 0; i < m_modes.size(); ++i) {
        const QColor c = m_modes[i]->isChecked() ? p.accent : p.text;
        m_modes[i]->setIcon(Theme::instance().icon(m_modeIcons[i], c));
    }
}
