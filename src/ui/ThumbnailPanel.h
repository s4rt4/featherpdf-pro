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

class ThumbnailModel;
class ThumbnailDelegate;
class QListView;
class QPdfDocument;

// The Thumbnails rail panel (ui-guidelines §5.5, §9). A vertical, lazily
// rendered list of page thumbnails: only on-screen pages are requested, and
// each is rendered off the UI thread (QPdfPageRenderer) so scrolling a 4000-page
// document never blocks. The active page carries a 2px accent frame; clicking a
// thumbnail navigates the viewport.
//
// This panel is render-layer code, so it is permitted to hold the QPdfDocument
// directly (the same exception the viewport uses).
class ThumbnailPanel : public QWidget {
    Q_OBJECT

public:
    explicit ThumbnailPanel(QWidget* parent = nullptr);

    void setDocument(QPdfDocument* doc);
    void setArrangement(const QVector<int>& order, const QVector<int>& rotations);
    void setRotation(int slot, int degrees);
    void clear();
    void setCurrentPage(int page); // highlight + scroll into view

signals:
    void pageActivated(int page);      // 0-based
    void pageMoved(int from, int to);  // drag-reorder request

private:
    QListView* m_view = nullptr;
    ThumbnailModel* m_model = nullptr;
    ThumbnailDelegate* m_delegate = nullptr;
};
