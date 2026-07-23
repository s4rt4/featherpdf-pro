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

#include "ui/NoteDialog.h"

#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

NoteDialog::NoteDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Add Note"));

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
    setStyleSheet(QStringLiteral("QPlainTextEdit { background:%1; border:1px solid %2;"
                                 " border-radius:8px; padding:6px 8px; color:%3;"
                                 " selection-background-color:%4; selection-color:#FFFFFF; }"
                                 "QPlainTextEdit:focus { border:1px solid %4; }")
                      .arg(css(p.surface), css(ctlBorder), css(p.text), css(p.accent)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);

    m_edit = new QPlainTextEdit(this);
    m_edit->setPlaceholderText(tr("Type your note…"));
    m_edit->setMinimumSize(320, 120);
    root->addWidget(m_edit);

    auto* buttons = new QDialogButtonBox(this);
    m_add = buttons->addButton(tr("Add Note"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    m_add->setObjectName(QStringLiteral("Share"));
    m_add->setCursor(Qt::PointingHandCursor);
    m_add->setEnabled(false);
    root->addWidget(buttons);

    connect(m_edit, &QPlainTextEdit::textChanged, this,
            [this] { m_add->setEnabled(!m_edit->toPlainText().trimmed().isEmpty()); });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_edit->setFocus();
}

QString NoteDialog::text() const {
    return m_edit->toPlainText().trimmed();
}
