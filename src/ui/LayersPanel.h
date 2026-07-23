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
#include <QWidget>
#include <memory>

namespace Poppler {
class Document;
} // namespace Poppler

class QTreeView;
class QStackedWidget;
class QLabel;
class QSortFilterProxyModel;

// The Layers rail panel: lists the document's optional content groups (OCGs)
// and their current visibility, via Poppler. The viewer renders through QtPdf,
// which exposes no API to toggle OCGs, so this panel is read-only - the check
// state shows each layer's stored visibility but cannot be changed here.
class LayersPanel : public QWidget {
    Q_OBJECT

public:
    explicit LayersPanel(QWidget* parent = nullptr);
    ~LayersPanel() override;

    void setDocumentPath(const QString& path);
    void clear();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void rebuild();

    QString m_path;
    bool m_dirty = false;
    std::unique_ptr<Poppler::Document> m_doc; // owns the optional-content model

    QSortFilterProxyModel* m_readOnly = nullptr;
    QTreeView* m_tree = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_empty = nullptr;
};
