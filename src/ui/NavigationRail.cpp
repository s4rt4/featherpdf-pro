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

#include "ui/NavigationRail.h"

#include "ui/Theme.h"

#include <QButtonGroup>
#include <QToolButton>
#include <QVBoxLayout>

NavigationRail::NavigationRail(QWidget* parent) : QWidget(parent) {
    setObjectName("NavigationRail");
    setFixedWidth(48);

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 8, 0, 8);
    col->setSpacing(4);
    col->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    col->addWidget(addButton(Panel::Thumbnails, "thumbnails", tr("Thumbnails")),
                   0, Qt::AlignHCenter);
    col->addWidget(addButton(Panel::Outline, "outline", tr("Outline")), 0, Qt::AlignHCenter);
    col->addWidget(addButton(Panel::Annotations, "edit", tr("Annotations")), 0, Qt::AlignHCenter);
    col->addWidget(addButton(Panel::Attachments, "attachment", tr("Attachments")),
                   0, Qt::AlignHCenter);
    col->addWidget(addButton(Panel::Layers, "layers", tr("Layers")), 0, Qt::AlignHCenter);
    col->addWidget(addButton(Panel::Forms, "form", tr("Forms")), 0, Qt::AlignHCenter);

    // Documentation - a plain action (not a panel) that opens a Docs tab.
    auto* docsBtn = new QToolButton(this);
    docsBtn->setToolTip(tr("Documentation"));
    docsBtn->setAccessibleName(tr("Documentation"));
    docsBtn->setCursor(Qt::PointingHandCursor);
    docsBtn->setFixedSize(34, 34);
    docsBtn->setIconSize(QSize(19, 19));
    docsBtn->setAutoRaise(true);
    connect(docsBtn, &QToolButton::clicked, this, &NavigationRail::docsRequested);
    m_iconButtons.append({docsBtn, QStringLiteral("help-circle")});
    col->addWidget(docsBtn, 0, Qt::AlignHCenter);

    refreshIcons();
    connect(&Theme::instance(), &Theme::changed, this, &NavigationRail::refreshIcons);
}

QToolButton* NavigationRail::addButton(Panel panel, const QString& iconName, const QString& tip) {
    auto* b = new QToolButton(this);
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(34, 34);
    b->setIconSize(QSize(19, 19));
    b->setAutoRaise(true);
    m_group->addButton(b);
    m_iconButtons.append({b, iconName});
    m_buttons.insert(panel, b);

    // Clicking the active panel again collapses it (mockup behaviour). Because the
    // group is exclusive we manage the toggle-off by hand.
    connect(b, &QToolButton::clicked, this, [this, b, panel](bool) {
        if (m_current == panel) {
            m_group->setExclusive(false);
            b->setChecked(false);
            m_group->setExclusive(true);
            m_current = Panel::None;
        } else {
            b->setChecked(true);
            m_current = panel;
        }
        refreshIcons();
        emit panelChanged(m_current);
    });
    return b;
}

void NavigationRail::setCurrentPanel(Panel panel) {
    if (panel == m_current)
        return;
    QToolButton* b = m_buttons.value(panel, nullptr);
    if (!b)
        return;
    b->setChecked(true);
    m_current = panel;
    refreshIcons();
    emit panelChanged(m_current);
}

void NavigationRail::refreshIcons() {
    const auto& pal = Theme::instance().palette();
    for (const auto& [btn, name] : m_iconButtons) {
        // Active panel icon picks up the accent; the rest stay dim (mockup .rbtn.on).
        const QColor c = btn->isChecked() ? pal.accent : pal.dim;
        btn->setIcon(Theme::instance().icon(name, c));
    }
}
