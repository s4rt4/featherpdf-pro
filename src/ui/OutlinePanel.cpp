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

#include "ui/OutlinePanel.h"

#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPdfBookmarkModel>
#include <QPdfDocument>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {
constexpr int kPageRole = Qt::UserRole + 1; // 0-based destination page on an item

QStandardItem* makeItem(const QString& title, int page) {
    auto* item = new QStandardItem(title);
    item->setData(page, kPageRole);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable |
                   Qt::ItemIsDragEnabled);
    return item;
}
} // namespace

OutlinePanel::OutlinePanel(QWidget* parent) : QWidget(parent) {
    setObjectName("OutlinePanel");

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    // Edit toolbar: Add / Rename / Delete on the left, Save on the right.
    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(8, 6, 8, 6);
    bar->setSpacing(6);
    // Icon-only toolbar (the rail is narrow); the tooltip names each action.
    const auto makeBtn = [this](const QString& tip) {
        auto* b = new QToolButton(this);
        b->setToolTip(tip);
        b->setAccessibleName(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setAutoRaise(true);
        b->setFixedSize(30, 30);
        b->setIconSize(QSize(17, 17));
        return b;
    };
    m_addBtn = makeBtn(tr("Add a bookmark to the current page"));
    m_renameBtn = makeBtn(tr("Rename the selected bookmark"));
    m_deleteBtn = makeBtn(tr("Delete the selected bookmark"));
    m_saveBtn = makeBtn(tr("Save the outline"));
    bar->addWidget(m_addBtn);
    bar->addWidget(m_renameBtn);
    bar->addWidget(m_deleteBtn);
    bar->addStretch(1);
    bar->addWidget(m_saveBtn);
    applyIcons();
    connect(&Theme::instance(), &Theme::changed, this, &OutlinePanel::applyIcons);
    col->addLayout(bar);

    m_stack = new QStackedWidget(this);

    m_tree = new QTreeView(this);
    m_tree->setObjectName("OutlineTree");
    m_tree->setHeaderHidden(true);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tree->setExpandsOnDoubleClick(false);
    m_tree->setAnimated(true);

    m_model = new QStandardItemModel(this);
    m_tree->setModel(m_model);

    m_empty = new QLabel(tr("No bookmarks yet. Use Add to create one."), this);
    m_empty->setObjectName("OutlineEmpty");
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->setContentsMargins(16, 16, 16, 16);

    m_stack->addWidget(m_tree);  // 0
    m_stack->addWidget(m_empty); // 1
    col->addWidget(m_stack, 1);

    connect(m_tree, &QTreeView::clicked, this, [this](const QModelIndex& index) {
        const int page = m_model->data(index, kPageRole).toInt();
        if (page >= 0)
            emit pageActivated(page);
    });
    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this] { updateButtons(); });
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &OutlinePanel::updateEmptyState);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, &OutlinePanel::updateEmptyState);

    connect(m_addBtn, &QToolButton::clicked, this, &OutlinePanel::addBookmark);
    connect(m_renameBtn, &QToolButton::clicked, this, &OutlinePanel::renameSelected);
    connect(m_deleteBtn, &QToolButton::clicked, this, &OutlinePanel::deleteSelected);
    connect(m_saveBtn, &QToolButton::clicked, this, &OutlinePanel::requestSave);

    updateEmptyState();
    updateButtons();
}

void OutlinePanel::setDocument(QPdfDocument* doc) {
    m_loaded = doc != nullptr;
    m_pageCount = doc ? doc->pageCount() : 0;
    m_currentPage = 0;
    populateFrom(doc);
    m_tree->expandToDepth(0); // top-level chapters open, deeper levels collapsed
    updateEmptyState();
    updateButtons();
}

void OutlinePanel::clear() {
    m_loaded = false;
    m_pageCount = 0;
    m_model->clear();
    updateEmptyState();
    updateButtons();
}

void OutlinePanel::setCurrentPage(int page) {
    m_currentPage = page;
}

void OutlinePanel::populateFrom(QPdfDocument* doc) {
    m_model->clear();
    if (!doc)
        return;

    // Copy the read-only bookmark tree (QPdfBookmarkModel) into our editable
    // model, preserving titles, destination pages, and nesting.
    QPdfBookmarkModel src;
    src.setDocument(doc);

    std::function<void(const QModelIndex&, QStandardItem*)> walk =
        [&](const QModelIndex& parent, QStandardItem* into) {
            const int rows = src.rowCount(parent);
            for (int r = 0; r < rows; ++r) {
                const QModelIndex idx = src.index(r, 0, parent);
                const QString title = src.data(idx, Qt::DisplayRole).toString();
                const int page = src.data(idx, int(QPdfBookmarkModel::Role::Page)).toInt();
                QStandardItem* item = makeItem(title, page);
                into->appendRow(item);
                walk(idx, item);
            }
        };
    walk(QModelIndex(), m_model->invisibleRootItem());
}

void OutlinePanel::addBookmark() {
    if (!m_loaded)
        return;
    const int page = m_pageCount > 0 ? std::max(0, std::min(m_currentPage, m_pageCount - 1)) : 0;
    QStandardItem* item = makeItem(tr("New bookmark"), page);
    m_model->invisibleRootItem()->appendRow(item); // new bookmarks land at the top level
    const QModelIndex idx = item->index();
    m_stack->setCurrentWidget(m_tree);
    m_tree->setCurrentIndex(idx);
    m_tree->scrollTo(idx);
    m_tree->edit(idx); // let the user type the title straight away
    updateButtons();
}

void OutlinePanel::renameSelected() {
    const QModelIndex idx = m_tree->currentIndex();
    if (idx.isValid())
        m_tree->edit(idx);
}

void OutlinePanel::deleteSelected() {
    const QModelIndex idx = m_tree->currentIndex();
    if (!idx.isValid())
        return;
    m_model->removeRow(idx.row(), idx.parent()); // removes the row and its children
    updateEmptyState();
    updateButtons();
}

void OutlinePanel::requestSave() {
    if (!m_loaded)
        return;
    std::function<QVector<PdfEditor::OutlineItem>(QStandardItem*)> serialize =
        [&](QStandardItem* parent) {
            QVector<PdfEditor::OutlineItem> out;
            const int rows = parent->rowCount();
            for (int r = 0; r < rows; ++r) {
                QStandardItem* child = parent->child(r);
                PdfEditor::OutlineItem it;
                it.title = child->text();
                it.page = child->data(kPageRole).toInt();
                it.children = serialize(child);
                out.push_back(it);
            }
            return out;
        };
    emit saveRequested(serialize(m_model->invisibleRootItem()));
}

void OutlinePanel::updateButtons() {
    const bool hasSel = m_loaded && m_tree->currentIndex().isValid();
    m_addBtn->setEnabled(m_loaded);
    m_saveBtn->setEnabled(m_loaded);
    m_renameBtn->setEnabled(hasSel);
    m_deleteBtn->setEnabled(hasSel);
}

void OutlinePanel::applyIcons() {
    const Theme::Palette& p = Theme::instance().palette();
    m_addBtn->setIcon(Theme::instance().icon(QStringLiteral("plus"), p.text));
    m_renameBtn->setIcon(Theme::instance().icon(QStringLiteral("edit"), p.text));
    m_deleteBtn->setIcon(Theme::instance().icon(QStringLiteral("x"), p.text));
    m_saveBtn->setIcon(Theme::instance().icon(QStringLiteral("save"), p.accent)); // primary
}

void OutlinePanel::updateEmptyState() {
    const bool hasItems = m_model->invisibleRootItem()->rowCount() > 0;
    m_stack->setCurrentWidget(hasItems ? static_cast<QWidget*>(m_tree)
                                       : static_cast<QWidget*>(m_empty));
}
