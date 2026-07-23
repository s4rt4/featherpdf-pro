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
#include <QVector>
#include <QWidget>

class QListWidget;
class QStackedWidget;
class QLabel;

// One existing annotation, flattened to plain data so it can cross threads.
struct AnnotationEntry {
    int page = 0;         // 0-based original page index
    QString kind;         // human label, e.g. "Highlight"
    QString text;         // the annotation's contents (may be empty)
};

// The Annotations rail panel: lists the annotations already present in the
// document (highlights, notes, ink, stamps, …) via Poppler, with the page they
// sit on; clicking one navigates the viewer there. Authoring new annotations is
// a later milestone (M3) - this is read-only. The scan runs off the UI thread
// so even a huge document stays responsive.
class AnnotationsPanel : public QWidget {
    Q_OBJECT

public:
    explicit AnnotationsPanel(QWidget* parent = nullptr);

    void setDocumentPath(const QString& path);
    void clear();

signals:
    void annotationActivated(int originalPage); // 0-based page index

protected:
    void showEvent(QShowEvent* event) override;

private:
    void rebuild();
    void populate(const QVector<AnnotationEntry>& entries);

    QString m_path;
    bool m_dirty = false;
    int m_scanGen = 0; // supersedes stale background scans

    QListWidget* m_list = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_empty = nullptr;
    QLabel* m_busy = nullptr;
};
