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

#include "ui/CustomizeToolsDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QHash>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int kIdRole = Qt::UserRole + 1;
}

CustomizeToolsDialog::CustomizeToolsDialog(const QVector<Tool>& catalog,
                                           const QStringList& visibleOrder,
                                           const QStringList& defaultOrder, QWidget* parent)
    : QDialog(parent), m_catalog(catalog), m_defaultOrder(defaultOrder) {
    setWindowTitle(tr("Customize Tools"));

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
        QStringLiteral("QLabel#Hint { color:%1; font-size:12px; }"
                       "QListWidget { background:%2; border:1px solid %3; border-radius:8px;"
                       " padding:4px; color:%4; outline:none; }"
                       "QListWidget::item { padding:5px 6px; border-radius:6px; }"
                       "QListWidget::item:selected { background:%5; color:%4; }")
            .arg(css(p.dim), css(p.surface), css(ctlBorder), css(p.text), css(p.accentTint)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    auto* hint = new QLabel(tr("Check the tools to show in the right pane, and drag to reorder."),
                            this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setMinimumHeight(320);
    populate(visibleOrder);
    root->addWidget(m_list, 1);

    auto* buttons = new QDialogButtonBox(this);
    auto* reset = buttons->addButton(tr("Reset"), QDialogButtonBox::ResetRole);
    auto* save = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    save->setObjectName(QStringLiteral("Share"));
    save->setCursor(Qt::PointingHandCursor);
    reset->setCursor(Qt::PointingHandCursor);
    connect(save, &QPushButton::clicked, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(reset, &QPushButton::clicked, this, [this] { populate(m_defaultOrder); });
    root->addWidget(buttons);

    resize(380, 460);
}

void CustomizeToolsDialog::populate(const QStringList& visibleOrder) {
    m_list->clear();
    const Theme::Palette& pal = Theme::instance().palette();

    QHash<QString, Tool> byId;
    for (const Tool& t : m_catalog)
        byId.insert(t.id, t);

    const auto addRow = [&](const Tool& t, bool checked) {
        auto* item = new QListWidgetItem(m_list);
        item->setText(t.label);
        item->setIcon(Theme::instance().icon(t.icon, pal.accent));
        item->setData(kIdRole, t.id);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable |
                       Qt::ItemIsDragEnabled);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    };

    // Visible tools first (in their order), then the rest (unchecked).
    for (const QString& id : visibleOrder)
        if (byId.contains(id))
            addRow(byId.value(id), true);
    for (const Tool& t : m_catalog)
        if (!visibleOrder.contains(t.id))
            addRow(t, false);
}

QStringList CustomizeToolsDialog::visibleOrderedIds() const {
    QStringList out;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem* item = m_list->item(i);
        if (item->checkState() == Qt::Checked)
            out << item->data(kIdRole).toString();
    }
    return out;
}
