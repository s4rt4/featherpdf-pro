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

#include "ui/FloatingPill.h"

#include "ui/Theme.h"

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>

FloatingPill::FloatingPill(QWidget* viewportParent) : QWidget(viewportParent) {
    setObjectName("FloatingPill");
    setAttribute(Qt::WA_StyledBackground, false);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(8, 5, 8, 5);
    row->setSpacing(4);

    auto* prev = addButton("chevron-left", tr("Previous page"));
    connect(prev, &QToolButton::clicked, this, &FloatingPill::prevPageRequested);
    row->addWidget(prev);

    m_page = new QLabel("- / -", this);
    m_page->setObjectName("PillLabel");
    m_page->setAlignment(Qt::AlignCenter);
    m_page->setCursor(Qt::PointingHandCursor);
    m_page->setToolTip(tr("Go to page"));
    m_page->installEventFilter(this); // click → goToPageRequested
    row->addWidget(m_page);

    auto* next = addButton("chevron-right", tr("Next page"));
    connect(next, &QToolButton::clicked, this, &FloatingPill::nextPageRequested);
    row->addWidget(next);

    row->addWidget(addSeparator());

    auto* zoomOut = addButton("minus", tr("Zoom out"));
    connect(zoomOut, &QToolButton::clicked, this, &FloatingPill::zoomOutRequested);
    row->addWidget(zoomOut);

    m_zoom = new QLabel("-", this);
    m_zoom->setObjectName("PillLabel");
    m_zoom->setAlignment(Qt::AlignCenter);
    row->addWidget(m_zoom);

    auto* zoomIn = addButton("plus", tr("Zoom in"));
    connect(zoomIn, &QToolButton::clicked, this, &FloatingPill::zoomInRequested);
    row->addWidget(zoomIn);

    row->addWidget(addSeparator());

    // Reading-mode toggle: its icon/tooltip flip via setReadingMode(). Kept out of
    // m_iconButtons so refreshIcons() can pick the state-dependent glyph.
    m_readingBtn = new QToolButton(this);
    m_readingBtn->setCursor(Qt::PointingHandCursor);
    m_readingBtn->setFixedSize(26, 26);
    m_readingBtn->setIconSize(QSize(15, 15));
    m_readingBtn->setAutoRaise(true);
    connect(m_readingBtn, &QToolButton::clicked, this, &FloatingPill::readingModeToggleRequested);
    row->addWidget(m_readingBtn);

    // The float shadow (ui-guidelines §4) - the pill is an overlay layer.
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 2);
    shadow->setColor(QColor(0, 0, 0, 46));
    setGraphicsEffect(shadow);

    viewportParent->installEventFilter(this);
    refreshIcons();
    connect(&Theme::instance(), &Theme::changed, this, [this] {
        refreshIcons();
        update();
    });
    reposition();
}

QToolButton* FloatingPill::addButton(const QString& iconName, const QString& tip) {
    auto* b = new QToolButton(this);
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(26, 26);
    b->setIconSize(QSize(15, 15));
    b->setAutoRaise(true);
    b->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:13px;}");
    m_iconButtons.append({b, iconName});
    return b;
}

QWidget* FloatingPill::addSeparator() {
    auto* sep = new QWidget(this);
    sep->setFixedSize(1, 16);
    sep->setStyleSheet(QStringLiteral("background:%1;")
                           .arg(Theme::instance().palette().hairline.name(QColor::HexArgb)));
    return sep;
}

void FloatingPill::refreshIcons() {
    const auto& pal = Theme::instance().palette();
    for (const auto& [btn, name] : m_iconButtons) {
        btn->setIcon(Theme::instance().icon(name, pal.text));
        // Re-tint the hover background per theme.
        btn->setStyleSheet(QStringLiteral("QToolButton{background:transparent;border:none;"
                                          "border-radius:13px;}"
                                          "QToolButton:hover{background:%1;}")
                               .arg(pal.canvas.name()));
    }
    if (m_readingBtn) {
        m_readingBtn->setIcon(Theme::instance().icon(
            m_readingMode ? QStringLiteral("reading-exit") : QStringLiteral("book"), pal.text));
        m_readingBtn->setToolTip(m_readingMode ? tr("Exit reading mode") : tr("Reading mode"));
        m_readingBtn->setStyleSheet(QStringLiteral("QToolButton{background:transparent;border:none;"
                                                   "border-radius:13px;}"
                                                   "QToolButton:hover{background:%1;}")
                                        .arg(pal.canvas.name()));
    }
    const QString labelCss = QStringLiteral("color:%1; font-family:'Source Code Pro',monospace;"
                                            " font-size:12px; padding:0 4px;")
                                 .arg(pal.text.name());
    if (m_page)
        m_page->setStyleSheet(labelCss);
    if (m_zoom)
        m_zoom->setStyleSheet(labelCss);
}

void FloatingPill::setReadingMode(bool on) {
    if (m_readingMode == on)
        return;
    m_readingMode = on;
    refreshIcons(); // swap the toggle glyph + tooltip
}

void FloatingPill::setPageText(const QString& text) {
    m_page->setText(text);
    reposition(); // the label may have grown - re-fit and re-center the pill
}

void FloatingPill::setZoomText(const QString& text) {
    m_zoom->setText(text);
    reposition();
}

void FloatingPill::reposition() {
    if (!parentWidget())
        return;
    adjustSize();
    const QRect pr = parentWidget()->rect();
    const int x = pr.center().x() - width() / 2;
    const int y = pr.bottom() - height() - 16;
    move(qMax(0, x), qMax(0, y));
    raise();
}

bool FloatingPill::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        reposition();
    if (watched == m_page && event->type() == QEvent::MouseButtonRelease) {
        emit goToPageRequested();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void FloatingPill::paintEvent(QPaintEvent*) {
    const auto& pal = Theme::instance().palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    path.addRoundedRect(r, height() / 2.0, height() / 2.0);
    p.fillPath(path, pal.surface);
    p.setPen(pal.hairline);
    p.drawPath(path);
}
