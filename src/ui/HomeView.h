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

#include <QWidget>

class QLabel;
class QVBoxLayout;
class QScrollArea;

class QPushButton;

// The Home start screen: a left sidebar (the brand, an Open action, and a
// "Recent files" nav item) beside a main area listing recent files as cards.
// The whole view is a drag-and-drop target. Reads recents from the same
// QSettings key the rest of the app uses; call refresh() after that list changes.
class HomeView : public QWidget {
    Q_OBJECT

public:
    explicit HomeView(QWidget* parent = nullptr);

    void refresh(); // rebuild the recent-files cards

signals:
    void openRequested();                        // the Open action
    void openPathRequested(const QString& path); // a recent card or a drop

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class ViewMode { List, Grid };
    void rebuildIcons();
    void setViewMode(ViewMode mode);
    int gridColumns() const; // columns that fit the current width in grid mode

    QLabel* m_logo = nullptr;
    QLabel* m_brandName = nullptr;
    QPushButton* m_openBtn = nullptr;
    QPushButton* m_recentNav = nullptr;
    QLabel* m_mainTitle = nullptr;
    QPushButton* m_listBtn = nullptr;
    QPushButton* m_gridBtn = nullptr;
    QLabel* m_empty = nullptr;
    QScrollArea* m_scroll = nullptr;
    QWidget* m_cards = nullptr; // re-created per refresh
    ViewMode m_viewMode = ViewMode::List;
    int m_lastColumns = 1;
    bool m_dragActive = false;
};
