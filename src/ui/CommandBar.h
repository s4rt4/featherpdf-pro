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

class QFrame;
class QLabel;
class QPushButton;
class QToolButton;

// The command toolbar (ui-guidelines §7 / mockup .toolbar): a single row scoped
// to the active document. Groups, left to right, separated by hairlines:
//   File actions (Save · Export · Print · Email) │ Find · ◀ counter ▶ │
//   zoom out · % · zoom in │ ⋯
// The page counter and zoom % are mono (the precise-value signal, §3).
// Doc-scoped buttons disable with no document.
class CommandBar : public QWidget {
    Q_OBJECT

public:
    explicit CommandBar(QWidget* parent = nullptr);

    void setPageText(const QString& text); // e.g. "3 / 24"
    void setZoomText(const QString& text); // e.g. "100%"
    void setDocumentLoaded(bool loaded);

signals:
    void saveRequested();
    void exportRequested();
    void printRequested();
    void emailRequested();
    void prevPageRequested();
    void nextPageRequested();
    void zoomOutRequested();
    void zoomInRequested();
    void moreRequested();
    void counterClicked(); // the page counter was clicked (go to page)

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QToolButton* addButton(const QString& iconName, const QString& tip);
    QFrame* addSeparator();
    void refreshIcons();

    QLabel* m_counter = nullptr;
    QLabel* m_zoom = nullptr;

    // (button, icon name) pairs so icons can be re-tinted on a theme change.
    QVector<QPair<QToolButton*, QString>> m_iconButtons;
    // Buttons that only make sense with a document open.
    QVector<QWidget*> m_docScoped;
};
