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

#include "ui/LayersPanel.h"

#include <QLabel>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QTreeView>
#include <QVBoxLayout>

#include <poppler-qt6.h>

namespace {
// Shows the optional-content model's check state but forbids toggling it: the
// QtPdf-based viewer can't honour OCG visibility changes, so the panel reports
// state without pretending to change the render.
class ReadOnlyCheckProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    Qt::ItemFlags flags(const QModelIndex& index) const override {
        return QSortFilterProxyModel::flags(index) &
               ~(Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
    }
};
} // namespace

LayersPanel::LayersPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("LayersPanel"));

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    m_stack = new QStackedWidget(this);

    m_tree = new QTreeView(this);
    m_tree->setObjectName(QStringLiteral("OutlineTree")); // reuse the flat-row styling
    m_tree->setHeaderHidden(true);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setSelectionMode(QAbstractItemView::NoSelection);
    m_tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tree->setFocusPolicy(Qt::NoFocus);

    m_readOnly = new ReadOnlyCheckProxy(this);
    m_tree->setModel(m_readOnly);

    m_empty = new QLabel(tr("This document has no layers."), this);
    m_empty->setObjectName(QStringLiteral("OutlineEmpty"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->setContentsMargins(16, 16, 16, 16);

    m_stack->addWidget(m_tree);  // 0
    m_stack->addWidget(m_empty); // 1
    col->addWidget(m_stack);
}

LayersPanel::~LayersPanel() = default;

void LayersPanel::setDocumentPath(const QString& path) {
    if (path == m_path && m_doc)
        return;
    m_path = path;
    m_dirty = true;
    if (isVisible())
        rebuild();
}

void LayersPanel::clear() {
    m_path.clear();
    m_dirty = false;
    m_readOnly->setSourceModel(nullptr);
    m_doc.reset();
    m_stack->setCurrentWidget(m_empty);
}

void LayersPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (m_dirty)
        rebuild();
}

void LayersPanel::rebuild() {
    m_dirty = false;
    m_readOnly->setSourceModel(nullptr);
    m_doc.reset();

    if (m_path.isEmpty()) {
        m_stack->setCurrentWidget(m_empty);
        return;
    }

    m_doc = Poppler::Document::load(m_path);
    if (!m_doc || m_doc->isLocked() || !m_doc->hasOptionalContent()) {
        m_stack->setCurrentWidget(m_empty);
        return;
    }

    // The model is owned by the document, which we keep alive in m_doc.
    m_readOnly->setSourceModel(m_doc->optionalContentModel());
    m_tree->expandAll();
    m_stack->setCurrentWidget(m_tree);
}
