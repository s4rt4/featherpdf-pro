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

#pragma once

#include <QHash>
#include <QVector>
#include <QWidget>

class QToolButton;
class QButtonGroup;

// The left navigation rail (ui-guidelines §5.5 / mockup .rail): a 48px column of
// icon-only toggles - Thumbnails · Outline · Annotations · Attachments · Layers.
// One panel at a time; clicking the active one again collapses it. The panels
// themselves arrive in a later increment; for now the rail owns the selection
// state and announces it.
class NavigationRail : public QWidget {
    Q_OBJECT

public:
    enum class Panel { None, Thumbnails, Outline, Annotations, Attachments, Layers, Forms };

    explicit NavigationRail(QWidget* parent = nullptr);

    Panel current() const { return m_current; }
    void setCurrentPanel(Panel panel); // open a panel programmatically

signals:
    void panelChanged(Panel panel); // Panel::None when collapsed
    void docsRequested();           // the documentation button (opens a Docs tab)

private:
    QToolButton* addButton(Panel panel, const QString& iconName, const QString& tip);
    void refreshIcons();

    QButtonGroup* m_group = nullptr;
    Panel m_current = Panel::None;
    QVector<QPair<QToolButton*, QString>> m_iconButtons;
    QHash<Panel, QToolButton*> m_buttons;
};
