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

#include <QVector>
#include <QWidget>

#include "backends/PdfEditor.h"

class QPdfDocument;
class QStandardItemModel;
class QStandardItem;
class QModelIndex;
class QTreeView;
class QStackedWidget;
class QLabel;
class QToolButton;

// The Outline rail panel (ui-guidelines §5.5): the document's bookmark tree,
// now editable. Bookmarks can be added (to the current page), renamed in place,
// and deleted; "Save" writes the new outline back through PdfEditor. Clicking an
// entry still navigates the viewport to its page. Existing nesting is preserved.
class OutlinePanel : public QWidget {
    Q_OBJECT

public:
    explicit OutlinePanel(QWidget* parent = nullptr);

    void setDocument(QPdfDocument* doc);
    void clear();
    void setCurrentPage(int page); // 0-based; where "Add bookmark" points

signals:
    void pageActivated(int page);                              // 0-based
    void saveRequested(const QVector<PdfEditor::OutlineItem>& items);

private:
    void addBookmark();
    void renameSelected();
    void deleteSelected();
    void requestSave();
    void updateButtons();
    void updateEmptyState();
    void applyIcons(); // (re)tint the toolbar icons for the current theme
    void populateFrom(QPdfDocument* doc);

    QStandardItemModel* m_model = nullptr;
    QTreeView* m_tree = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_empty = nullptr;
    QToolButton* m_addBtn = nullptr;
    QToolButton* m_renameBtn = nullptr;
    QToolButton* m_deleteBtn = nullptr;
    QToolButton* m_saveBtn = nullptr;
    int m_currentPage = 0;
    int m_pageCount = 0;
    bool m_loaded = false;
};
