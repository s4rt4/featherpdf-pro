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

#include "ui/TextEditDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

TextEditDialog::TextEditDialog(const QList<TextEditor::TextBox>& boxes, QWidget* parent)
    : QDialog(parent), m_boxes(boxes) {
    setWindowTitle(tr("Edit Text"));

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
                       "QListWidget, QPlainTextEdit { background:%2; border:1px solid %3;"
                       " border-radius:8px; padding:4px; color:%4; }"
                       "QPlainTextEdit:focus, QListWidget:focus { border:1px solid %5; }")
            .arg(css(p.dim), css(p.surface), css(ctlBorder), css(p.text), css(p.accent)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(8);

    auto* hint = new QLabel(tr("Edit the text boxes you've added. Pick one, change its words, "
                               "and Save, or Delete it."),
                            this);
    hint->setObjectName(QStringLiteral("Hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    m_list = new QListWidget(this);
    for (const TextEditor::TextBox& b : m_boxes) {
        QString snippet = b.text.simplified();
        if (snippet.size() > 48)
            snippet = snippet.left(45) + QStringLiteral("…");
        if (snippet.isEmpty())
            snippet = tr("(empty)");
        m_list->addItem(tr("Page %1: %2").arg(b.page + 1).arg(snippet));
    }
    m_list->setMinimumHeight(120);
    root->addWidget(m_list, 1);

    m_editor = new QPlainTextEdit(this);
    m_editor->setMinimumHeight(70);
    root->addWidget(m_editor);

    auto* buttons = new QDialogButtonBox(this);
    auto* save = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    auto* del = buttons->addButton(tr("Delete"), QDialogButtonBox::DestructiveRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    save->setObjectName(QStringLiteral("Share"));
    save->setCursor(Qt::PointingHandCursor);
    del->setCursor(Qt::PointingHandCursor);
    root->addWidget(buttons);

    connect(m_list, &QListWidget::currentRowChanged, this, [this] { syncToSelection(); });
    connect(save, &QPushButton::clicked, this, [this] {
        if (m_list->currentRow() >= 0) {
            m_action = Action::Save;
            accept();
        }
    });
    connect(del, &QPushButton::clicked, this, [this] {
        if (m_list->currentRow() >= 0) {
            m_action = Action::Delete;
            accept();
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    resize(440, 380);
    if (!m_boxes.isEmpty())
        m_list->setCurrentRow(0);
    syncToSelection();
}

void TextEditDialog::syncToSelection() {
    const int row = m_list->currentRow();
    if (row >= 0 && row < m_boxes.size())
        m_editor->setPlainText(m_boxes[row].text);
    m_editor->setEnabled(row >= 0);
}

int TextEditDialog::selectedIndex() const {
    return m_list->currentRow();
}

QString TextEditDialog::text() const {
    return m_editor->toPlainText();
}
