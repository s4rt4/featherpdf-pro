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

#include <QColor>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;

// The annotation action strip, shown beneath the command bar while highlight
// mode is active: a hint, a live mark count, and Save / Clear / Done. The marks
// are drawn by the page view; this bar only drives the actions.
class AnnotationBar : public QWidget {
    Q_OBJECT

public:
    explicit AnnotationBar(QWidget* parent = nullptr);

    void setCount(int count); // updates the label and enables/disables Save

signals:
    void saveRequested();
    void clearRequested();
    void doneRequested();
    void toolChanged(int tool); // 0 = Highlight, 1 = Note
    void colorChanged(const QColor& color);

private:
    void selectSwatch(int index);
    void applyToolIcons(); // re-tint tool icons (checked = accent, rest = text)

    QLabel* m_label = nullptr;
    QPushButton* m_info = nullptr; // "i" button; its tooltip carries the usage hint
    QVector<QPushButton*> m_tools; // the 9 tool buttons, in toolChanged() index order
    QStringList m_toolIcons;       // symbolic icon name per tool (parallel to m_tools)
    QPushButton* m_save = nullptr;
    QPushButton* m_clear = nullptr;
    QVector<QPushButton*> m_swatches;
    QVector<QColor> m_colors;
};
