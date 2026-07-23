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

#include "ui/CommandBar.h"

#include "ui/Theme.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>

CommandBar::CommandBar(QWidget* parent) : QWidget(parent) {
    setObjectName("CommandBar");
    setFixedHeight(44);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(12, 7, 12, 7);
    row->setSpacing(6);

    // 1. File actions.
    auto* save = addButton("save", tr("Save"));
    connect(save, &QToolButton::clicked, this, &CommandBar::saveRequested);
    auto* exp = addButton("export", tr("Export"));
    connect(exp, &QToolButton::clicked, this, &CommandBar::exportRequested);
    auto* prn = addButton("print", tr("Print"));
    connect(prn, &QToolButton::clicked, this, &CommandBar::printRequested);
    auto* mail = addButton("mail", tr("Email"));
    connect(mail, &QToolButton::clicked, this, &CommandBar::emailRequested);
    m_docScoped += {save, exp, prn, mail};
    for (auto* b : {save, exp, prn, mail})
        row->addWidget(b);

    row->addWidget(addSeparator());

    // 2. Navigate. (In-document search lives in the tab strip's search button.)
    auto* prev = addButton("chevron-up", tr("Previous page"));
    connect(prev, &QToolButton::clicked, this, &CommandBar::prevPageRequested);
    row->addWidget(prev);

    m_counter = new QLabel("- / -", this);
    m_counter->setObjectName("Counter");
    m_counter->setAlignment(Qt::AlignCenter);
    m_counter->setMinimumWidth(52);
    m_counter->setCursor(Qt::PointingHandCursor);
    m_counter->setToolTip(tr("Go to page"));
    m_counter->installEventFilter(this); // click to jump to a page
    row->addWidget(m_counter);

    auto* next = addButton("chevron-down", tr("Next page"));
    connect(next, &QToolButton::clicked, this, &CommandBar::nextPageRequested);
    row->addWidget(next);
    m_docScoped += {prev, next};

    row->addWidget(addSeparator());

    // 3. Zoom.
    auto* zoomOut = addButton("minus", tr("Zoom out"));
    connect(zoomOut, &QToolButton::clicked, this, &CommandBar::zoomOutRequested);
    row->addWidget(zoomOut);

    m_zoom = new QLabel("-", this);
    m_zoom->setObjectName("Zoom");
    m_zoom->setAlignment(Qt::AlignCenter);
    m_zoom->setMinimumWidth(46);
    row->addWidget(m_zoom);

    auto* zoomIn = addButton("plus", tr("Zoom in"));
    connect(zoomIn, &QToolButton::clicked, this, &CommandBar::zoomInRequested);
    row->addWidget(zoomIn);

    auto* more = addButton("dots-horizontal", tr("More"));
    connect(more, &QToolButton::clicked, this, &CommandBar::moreRequested);
    row->addWidget(more);
    m_docScoped += {zoomOut, zoomIn, more};

    row->addStretch(1);

    refreshIcons();
    connect(&Theme::instance(), &Theme::changed, this, &CommandBar::refreshIcons);
    setDocumentLoaded(false);
}

QToolButton* CommandBar::addButton(const QString& iconName, const QString& tip) {
    auto* b = new QToolButton(this);
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(30, 30);
    b->setIconSize(QSize(17, 17));
    b->setAutoRaise(true);
    m_iconButtons.append({b, iconName});
    return b;
}

QFrame* CommandBar::addSeparator() {
    auto* sep = new QFrame(this);
    sep->setObjectName("Sep");
    sep->setFixedSize(1, 20);
    return sep;
}

void CommandBar::refreshIcons() {
    const QColor c = Theme::instance().palette().text;
    for (const auto& [btn, name] : m_iconButtons)
        btn->setIcon(Theme::instance().icon(name, c));
}

bool CommandBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_counter && event->type() == QEvent::MouseButtonRelease &&
        m_counter->isEnabled())
        emit counterClicked();
    return QWidget::eventFilter(watched, event);
}

void CommandBar::setPageText(const QString& text) {
    m_counter->setText(text);
}

void CommandBar::setZoomText(const QString& text) {
    m_zoom->setText(text);
}

void CommandBar::setDocumentLoaded(bool loaded) {
    for (auto* w : m_docScoped)
        w->setEnabled(loaded);
    if (!loaded) {
        m_counter->setText("- / -");
        m_zoom->setText("-");
    }
}
