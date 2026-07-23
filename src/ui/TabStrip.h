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
#include <QWidget>

class Tab;
class QHBoxLayout;
class QToolButton;

// The tab strip (ui-guidelines §6 / mockup .tabs) - the defining element. Two
// fixed mode tabs (Home · Tools) are pinned left, then one document tab per open
// PDF, then a "+". The active tab fills with `surface`, carries a 2px accent top
// indicator, and connects seamlessly to the command toolbar below. Right of the
// strip: quick search, a light/dark affordance, and the app menu.
//
// Document tabs are dynamic: each open file gets its own tab, identified by an
// integer id the caller maps to its document session.
class TabStrip : public QWidget {
    Q_OBJECT

public:
    explicit TabStrip(QWidget* parent = nullptr);

    // Document-tab management. addDocument returns a new id; the rest address a
    // tab by that id.
    int addDocument(const QString& title);
    void removeDocument(int id);
    void setActiveDocument(int id);
    void setDocumentTitle(int id, const QString& title);
    void setDocumentDirty(int id, bool dirty);

    void setActiveHome(); // make the Home tab the active one
    int documentCount() const { return m_docTabs.size(); }

    // The Documentation tab - a single special tab created on demand.
    void showDocsTab();    // create (if needed) and make it active
    void closeDocsTab();   // remove it
    void setActiveDocs();  // mark it active without recreating

signals:
    void homeSelected();
    void documentSelected(int id);
    void newTabRequested();
    void closeDocumentRequested(int id);
    void docsSelected();
    void docsCloseRequested();
    void searchRequested();
    void menuRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QToolButton* makeRightButton(const QString& iconName, const QString& tip);
    void refreshIcons();
    void clearActiveStates();

    QHBoxLayout* m_layout = nullptr;
    Tab* m_home = nullptr;
    Tab* m_docs = nullptr;
    Tab* m_add = nullptr;
    QToolButton* m_search = nullptr;
    QToolButton* m_theme = nullptr;
    QToolButton* m_menu = nullptr;

    QHash<int, Tab*> m_docTabs;
    int m_nextId = 1;
};
