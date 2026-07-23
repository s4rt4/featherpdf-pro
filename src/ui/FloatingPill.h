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

class QLabel;
class QToolButton;

// The floating page/zoom pill (ui-guidelines §1, §5.6 / mockup .pill) - the
// signature overlay control. It sits bottom-center over the viewport: ◀ page ▶,
// then zoom out / % / zoom in, on a rounded `surface` pill with the float
// shadow. Parented to the viewport widget; it keeps itself centered as that
// widget resizes via an event filter.
class FloatingPill : public QWidget {
    Q_OBJECT

public:
    explicit FloatingPill(QWidget* viewportParent);

    void setPageText(const QString& text);
    void setZoomText(const QString& text);
    void setReadingMode(bool on); // swaps the reading-mode toggle icon/tooltip

signals:
    void prevPageRequested();
    void nextPageRequested();
    void zoomOutRequested();
    void zoomInRequested();
    void goToPageRequested();          // the page label was clicked
    void readingModeToggleRequested(); // the reading-mode button was clicked

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QToolButton* addButton(const QString& iconName, const QString& tip);
    QWidget* addSeparator();
    void reposition();
    void refreshIcons();

    QLabel* m_page = nullptr;
    QLabel* m_zoom = nullptr;
    QToolButton* m_readingBtn = nullptr;
    bool m_readingMode = false;
    QVector<QPair<QToolButton*, QString>> m_iconButtons;
};
