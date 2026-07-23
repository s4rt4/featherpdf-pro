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

#include "ui/Toast.h"

#include "ui/Theme.h"

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

Toast::Toast(QWidget* host) : QWidget(host) {
    setObjectName("Toast");
    setAttribute(Qt::WA_TransparentForMouseEvents, true); // never blocks the page
    hide();

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(16, 9, 16, 9);
    m_label = new QLabel(this);
    row->addWidget(m_label);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 60));
    setGraphicsEffect(shadow);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &QWidget::hide);

    if (host)
        host->installEventFilter(this);
}

void Toast::show(const QString& message, int milliseconds) {
    const auto& pal = Theme::instance().palette();
    m_label->setStyleSheet(QStringLiteral("color:%1; font-size:13px;").arg(pal.text.name()));
    m_label->setText(message);
    adjustSize();
    reposition();
    raise();
    QWidget::show();
    m_timer->start(milliseconds);
}

void Toast::reposition() {
    if (!parentWidget())
        return;
    adjustSize();
    const QRect pr = parentWidget()->rect();
    // Sit above where the floating pill lives, so the two never collide.
    move(pr.center().x() - width() / 2, pr.bottom() - height() - 64);
}

bool Toast::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible())
        reposition();
    return QWidget::eventFilter(watched, event);
}

void Toast::paintEvent(QPaintEvent*) {
    const auto& pal = Theme::instance().palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    path.addRoundedRect(r, 10, 10);
    p.fillPath(path, pal.surface);
    p.setPen(pal.hairline);
    p.drawPath(path);
}
