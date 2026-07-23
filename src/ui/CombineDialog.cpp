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

#include "ui/CombineDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

CombineDialog::CombineDialog(const QString& initialPath, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Combine PDFs"));

    const Theme::Palette& p = Theme::instance().palette();
    QColor ctlBorder = p.text;
    ctlBorder.setAlpha(46);
    const auto css = [](const QColor& c) {
        return c.alpha() == 255 ? c.name(QColor::HexRgb)
                                : QStringLiteral("rgba(%1,%2,%3,%4)")
                                      .arg(c.red())
                                      .arg(c.green())
                                      .arg(c.blue())
                                      .arg(QString::number(c.alphaF(), 'f', 3));
    };
    setStyleSheet(
        QStringLiteral(
            // Buttons (#GhostBtn, button box) inherit the neutral hierarchy from
            // the global sheet; only the list needs dialog-local styling.
            "QLabel#Hint { color:%2; }"
            "#CombineList { background:%1; border:1px solid %3; border-radius:8px; outline:0; }"
            "#CombineList::item { padding:7px 10px; color:%4; border-radius:6px; }"
            "#CombineList::item:selected { background:%5; color:%4; }")
            .arg(css(p.surface), css(p.dim), css(ctlBorder), css(p.text), css(p.accentTint)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(12);

    auto* hint = new QLabel(tr("Files are merged top to bottom. Drag to reorder."), this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* row = new QHBoxLayout;
    row->setSpacing(12);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("CombineList"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setMinimumSize(360, 240);
    row->addWidget(m_list, 1);

    // Side controls.
    auto* side = new QVBoxLayout;
    side->setSpacing(8);
    auto* add = new QPushButton(tr("Add files…"), this);
    m_remove = new QPushButton(tr("Remove"), this);
    m_up = new QPushButton(tr("Move up"), this);
    m_down = new QPushButton(tr("Move down"), this);
    for (QPushButton* b : {add, m_remove, m_up, m_down}) {
        b->setObjectName(QStringLiteral("GhostBtn"));
        b->setCursor(Qt::PointingHandCursor);
        side->addWidget(b);
    }
    side->addStretch(1);
    row->addLayout(side);
    root->addLayout(row);

    auto* buttons = new QDialogButtonBox(this);
    m_combine = buttons->addButton(tr("Combine"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    m_combine->setObjectName(QStringLiteral("Share")); // accent-filled primary
    m_combine->setCursor(Qt::PointingHandCursor);
    root->addWidget(buttons);

    connect(add, &QPushButton::clicked, this, &CombineDialog::addFiles);
    connect(m_remove, &QPushButton::clicked, this, &CombineDialog::removeSelected);
    connect(m_up, &QPushButton::clicked, this, [this] { moveSelected(-1); });
    connect(m_down, &QPushButton::clicked, this, [this] { moveSelected(1); });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &CombineDialog::updateState);
    connect(m_list->model(), &QAbstractItemModel::rowsMoved, this, &CombineDialog::updateState);

    if (!initialPath.isEmpty())
        addPath(initialPath);
    updateState();
    resize(560, 380);
}

void CombineDialog::addPath(const QString& path) {
    auto* item = new QListWidgetItem(QFileInfo(path).fileName(), m_list);
    item->setData(Qt::UserRole, QFileInfo(path).absoluteFilePath());
    item->setToolTip(QFileInfo(path).absoluteFilePath());
}

void CombineDialog::addFiles() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Add PDFs to combine"), QString(), tr("PDF documents (*.pdf)"));
    for (const QString& f : files)
        addPath(f);
    updateState();
}

void CombineDialog::removeSelected() {
    qDeleteAll(m_list->selectedItems());
    updateState();
}

void CombineDialog::moveSelected(int delta) {
    const int row = m_list->currentRow();
    if (row < 0)
        return;
    const int dest = row + delta;
    if (dest < 0 || dest >= m_list->count())
        return;
    QListWidgetItem* item = m_list->takeItem(row);
    m_list->insertItem(dest, item);
    m_list->setCurrentRow(dest);
    updateState();
}

void CombineDialog::updateState() {
    const int count = m_list->count();
    const int row = m_list->currentRow();
    m_combine->setEnabled(count >= 2);
    m_remove->setEnabled(row >= 0);
    m_up->setEnabled(row > 0);
    m_down->setEnabled(row >= 0 && row < count - 1);
}

QStringList CombineDialog::paths() const {
    QStringList out;
    for (int i = 0; i < m_list->count(); ++i)
        out << m_list->item(i)->data(Qt::UserRole).toString();
    return out;
}
