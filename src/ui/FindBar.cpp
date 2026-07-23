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

#include "ui/FindBar.h"

#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

FindBar::FindBar(QWidget* parent) : QWidget(parent) {
    setObjectName("FindBar");
    setFixedHeight(44);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(12, 6, 12, 6);
    row->setSpacing(6);

    auto* icon = new QLabel(this);
    icon->setObjectName("FindIcon");
    icon->setFixedSize(18, 18);
    row->addWidget(icon);

    m_input = new QLineEdit(this);
    m_input->setObjectName("FindInput");
    m_input->setPlaceholderText(tr("Find in document"));
    m_input->setClearButtonEnabled(true);
    m_input->setFixedWidth(280);
    connect(m_input, &QLineEdit::textChanged, this, &FindBar::queryChanged);
    connect(m_input, &QLineEdit::returnPressed, this, &FindBar::findNext);
    row->addWidget(m_input);

    m_count = new QLabel(this);
    m_count->setObjectName("FindCount");
    m_count->setMinimumWidth(64);
    row->addWidget(m_count);

    auto* prev = addButton("chevron-up", tr("Previous match"));
    connect(prev, &QToolButton::clicked, this, &FindBar::findPrevious);
    row->addWidget(prev);

    auto* next = addButton("chevron-down", tr("Next match"));
    connect(next, &QToolButton::clicked, this, &FindBar::findNext);
    row->addWidget(next);

    row->addSpacing(8);
    auto* close = addButton("x", tr("Close find"));
    connect(close, &QToolButton::clicked, this, &FindBar::closed);
    row->addWidget(close);

    // Keep the whole group compact on the left.
    row->addStretch(1);

    refreshIcons();
    connect(&Theme::instance(), &Theme::changed, this, &FindBar::refreshIcons);
}

QToolButton* FindBar::addButton(const QString& iconName, const QString& tip) {
    auto* b = new QToolButton(this);
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(28, 28);
    b->setIconSize(QSize(16, 16));
    b->setAutoRaise(true);
    m_iconButtons.append({b, iconName});
    return b;
}

void FindBar::refreshIcons() {
    const auto& pal = Theme::instance().palette();
    for (const auto& [btn, name] : m_iconButtons)
        btn->setIcon(Theme::instance().icon(name, pal.text));
    if (auto* icon = findChild<QLabel*>("FindIcon"))
        icon->setPixmap(Theme::instance().icon("search", pal.dim).pixmap(16, 16));
}

void FindBar::activate() {
    show();
    m_input->setFocus();
    m_input->selectAll();
}

void FindBar::setResult(int current, int total) {
    if (m_input->text().isEmpty())
        m_count->setText(QString());
    else if (total <= 0)
        m_count->setText(tr("No results"));
    else
        m_count->setText(tr("%1 of %2").arg(current).arg(total));
}

QString FindBar::query() const {
    return m_input->text();
}

void FindBar::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        emit closed();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (event->modifiers() & Qt::ShiftModifier)
            emit findPrevious();
        else
            emit findNext();
        return;
    }
    QWidget::keyPressEvent(event);
}
