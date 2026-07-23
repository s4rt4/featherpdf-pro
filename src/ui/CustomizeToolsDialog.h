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

#include <QDialog>
#include <QStringList>
#include <QVector>

class QListWidget;

// Picks which tools appear in the right Tools pane, and in what order. Every
// tool is a checkable, drag-reorderable row; checked rows (top to bottom) become
// the pane's contents.
class CustomizeToolsDialog : public QDialog {
    Q_OBJECT

public:
    struct Tool {
        QString id;
        QString label;
        QString icon;
    };

    // `catalog` is every available tool; `visibleOrder` the currently-shown ids
    // in their display order. `defaultOrder` powers the Reset button.
    CustomizeToolsDialog(const QVector<Tool>& catalog, const QStringList& visibleOrder,
                         const QStringList& defaultOrder, QWidget* parent = nullptr);

    // Checked tool ids, top to bottom — the new pane contents.
    QStringList visibleOrderedIds() const;

private:
    void populate(const QStringList& visibleOrder);

    QVector<Tool> m_catalog;
    QStringList m_defaultOrder;
    QListWidget* m_list = nullptr;
};
