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

#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class ToolRow;
class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;

// The right Tools pane (ui-guidelines §8 / mockup .tools): a vertical list of
// tool rows, each an accent-tinted icon chip + label, some expandable (chevron).
// Default tools: Export · Create · Edit · Comment · Combine · Organize · Redact ·
// Sign. No upsell - the footer is "Customize tools". Each tool's own panel
// replaces this list in a later increment; for now selecting a row announces it.
class ToolsPane : public QWidget {
    Q_OBJECT

public:
    explicit ToolsPane(QWidget* parent = nullptr);

signals:
    void toolActivated(const QString& id);

private:
    struct ToolDef {
        QString id;
        QString label;
        QString icon;
        bool expandable;
    };

    void refreshIcons();
    void setCollapsed(bool collapsed); // icon-only when collapsed
    void openCustomize();              // the "Customize tools" dialog
    void applyOrder();                 // show/hide + reorder rows per m_order
    void loadOrder();                  // read m_order from QSettings (or default)
    void saveOrder() const;            // persist m_order to QSettings
    QStringList defaultOrder() const;  // every tool id in catalog order

    QVector<ToolDef> m_catalog;
    QStringList m_order; // visible tool ids, in display order
    QVector<ToolRow*> m_rows;
    QVBoxLayout* m_col = nullptr;
    QLabel* m_head = nullptr;
    QToolButton* m_toggle = nullptr;
    QPushButton* m_customize = nullptr;
    bool m_collapsed = false;
};
