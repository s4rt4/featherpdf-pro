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

#include "ui/TabStrip.h"

#include "ui/Theme.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>

namespace {
constexpr int kTabHeight = 34;
constexpr int kStripTop = 10; // breathing room between the menu bar and the tabs
constexpr int kPadH = 13;     // horizontal text padding
constexpr int kGap = 7;       // gap between icon/label/extras
constexpr int kIcon = 15;     // leading icon (Home/Tools)
constexpr int kClose = 16;    // close hit box
constexpr int kDot = 6;       // dirty dot
} // namespace

// A single tab, custom-painted so the active tab can carry the 2px accent top
// indicator and merge seamlessly with the command toolbar below.
class Tab : public QWidget {
    Q_OBJECT

public:
    enum class Kind { Home, Tools, Docs, Document, Add };

    Tab(Kind kind, QString iconName, QString label, QWidget* parent)
        : QWidget(parent), m_kind(kind), m_iconName(std::move(iconName)),
          m_label(std::move(label)) {
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(kTabHeight);
        setAttribute(Qt::WA_Hover, true);
    }

    void setActive(bool a) {
        if (m_active == a)
            return;
        m_active = a;
        update();
    }
    void setLabel(const QString& l) {
        m_label = l;
        updateGeometry();
        update();
    }
    void setDirty(bool d) {
        if (m_dirty == d)
            return;
        m_dirty = d;
        updateGeometry();
        update();
    }
    bool closable() const { return m_kind == Kind::Document || m_kind == Kind::Docs; }

    QSize sizeHint() const override {
        if (m_kind == Kind::Add)
            return {34, kTabHeight};
        const QFontMetrics fm(font());
        int w = kPadH;
        if (m_kind == Kind::Home || m_kind == Kind::Tools || m_kind == Kind::Docs)
            w += kIcon + kGap;
        w += fm.horizontalAdvance(m_label);
        if (m_dirty)
            w += kGap + kDot;
        if (closable())
            w += kGap + kClose;
        w += kPadH;
        return {w, kTabHeight};
    }

signals:
    void clicked();
    void closeClicked();

protected:
    void enterEvent(QEnterEvent*) override {
        m_hover = true;
        update();
    }
    void leaveEvent(QEvent*) override {
        m_hover = false;
        m_closeHover = false;
        update();
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        const bool ch = closeRect().contains(e->pos());
        if (ch != m_closeHover) {
            m_closeHover = ch;
            update();
        }
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() != Qt::LeftButton)
            return;
        if (closable() && closeRect().contains(e->pos()))
            emit closeClicked();
        else if (rect().contains(e->pos()))
            emit clicked();
    }

    void paintEvent(QPaintEvent*) override {
        const auto& pal = Theme::instance().palette();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF r = rect();
        // Rounded-top background: surface when active, faint tonal on hover.
        if (m_active || m_hover) {
            QPainterPath path;
            const qreal rad = 8;
            path.moveTo(r.left(), r.bottom());
            path.lineTo(r.left(), r.top() + rad);
            path.quadTo(r.left(), r.top(), r.left() + rad, r.top());
            path.lineTo(r.right() - rad, r.top());
            path.quadTo(r.right(), r.top(), r.right(), r.top() + rad);
            path.lineTo(r.right(), r.bottom());
            QColor bg = m_active ? pal.surface
                                 : QColor(pal.text.red(), pal.text.green(), pal.text.blue(),
                                          Theme::instance().mode() == Theme::Mode::Dark ? 13 : 10);
            p.fillPath(path, bg);
        }

        // 2px accent indicator along the active tab's top edge (inset 8px).
        if (m_active) {
            QPainterPath bar;
            bar.addRoundedRect(QRectF(r.left() + 8, r.top(), r.width() - 16, 2), 1, 1);
            p.fillPath(bar, pal.accent);
        }

        const QColor fg = m_active ? pal.text : (m_hover ? pal.text : pal.dim);
        int x = kPadH;

        if (m_kind == Kind::Add) {
            const QPixmap pm = Theme::instance().icon("plus", fg).pixmap(14, 14);
            p.drawPixmap((width() - 14) / 2, (height() - 14) / 2, pm);
            return;
        }

        if (m_kind == Kind::Home || m_kind == Kind::Tools || m_kind == Kind::Docs) {
            const QPixmap pm = Theme::instance().icon(m_iconName, fg).pixmap(kIcon, kIcon);
            p.drawPixmap(x, (height() - kIcon) / 2, pm);
            x += kIcon + kGap;
        }

        // Label (active is semibold).
        QFont f = font();
        f.setWeight(m_active ? QFont::DemiBold : QFont::Normal);
        p.setFont(f);
        p.setPen(fg);
        const QFontMetrics fm(f);
        p.drawText(QRect(x, 0, fm.horizontalAdvance(m_label), height()),
                   Qt::AlignVCenter | Qt::AlignLeft, m_label);
        x += fm.horizontalAdvance(m_label);

        if (m_dirty) {
            x += kGap;
            p.setBrush(pal.warning);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPoint(x + kDot / 2, height() / 2), kDot / 2, kDot / 2);
            x += kDot;
        }

        // Close affordance: visible when active or hovered (document tabs only).
        if (closable() && (m_active || m_hover)) {
            const QRect cr = closeRect();
            if (m_closeHover) {
                QPainterPath cp;
                cp.addRoundedRect(cr, 4, 4);
                p.fillPath(cp, pal.hairline);
            }
            const QColor xcol = m_closeHover ? pal.text : pal.dim;
            const QPixmap pm = Theme::instance().icon("x", xcol).pixmap(11, 11);
            p.drawPixmap(cr.center().x() - 5, cr.center().y() - 5, pm);
        }
    }

private:
    QRect closeRect() const {
        return QRect(width() - kPadH - kClose, (height() - kClose) / 2, kClose, kClose);
    }

    Kind m_kind;
    QString m_iconName;
    QString m_label;
    bool m_active = false;
    bool m_hover = false;
    bool m_closeHover = false;
    bool m_dirty = false;
};

TabStrip::TabStrip(QWidget* parent) : QWidget(parent) {
    setObjectName("TabStrip");
    setAutoFillBackground(false);
    setFixedHeight(kTabHeight + kStripTop);

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(8, kStripTop, 8, 0);
    m_layout->setSpacing(2);
    m_layout->setAlignment(Qt::AlignBottom | Qt::AlignLeft);

    m_home = new Tab(Tab::Kind::Home, "home", tr("Home"), this);
    m_add = new Tab(Tab::Kind::Add, QString(), QString(), this);
    m_add->setToolTip(tr("Open a document in a new tab"));

    // Document tabs are inserted between Home and the "+".
    m_layout->addWidget(m_home);
    m_layout->addWidget(m_add);
    m_layout->addStretch(1);

    connect(m_home, &Tab::clicked, this, [this] { emit homeSelected(); });
    connect(m_add, &Tab::clicked, this, &TabStrip::newTabRequested);

    // Right-side controls: search · theme · menu.
    m_search = makeRightButton("search", tr("Search"));
    connect(m_search, &QToolButton::clicked, this, &TabStrip::searchRequested);
    m_theme = makeRightButton("sun", tr("Toggle light / dark"));
    connect(m_theme, &QToolButton::clicked, this, [this] {
        Theme::instance().toggleMode();
        refreshIcons();
    });
    m_menu = makeRightButton("dots-vertical", tr("Menu"));
    connect(m_menu, &QToolButton::clicked, this, &TabStrip::menuRequested);
    for (QToolButton* b : {m_search, m_theme, m_menu})
        m_layout->addWidget(b, 0, Qt::AlignBottom);

    setActiveHome();
    refreshIcons();
    connect(&Theme::instance(), &Theme::changed, this, &TabStrip::refreshIcons);
}

int TabStrip::addDocument(const QString& title) {
    const int id = m_nextId++;
    auto* tab = new Tab(Tab::Kind::Document, QString(), title, this);
    m_docTabs.insert(id, tab);
    m_layout->insertWidget(m_layout->indexOf(m_add), tab);

    connect(tab, &Tab::clicked, this, [this, id] { emit documentSelected(id); });
    connect(tab, &Tab::closeClicked, this, [this, id] { emit closeDocumentRequested(id); });
    return id;
}

void TabStrip::removeDocument(int id) {
    if (Tab* tab = m_docTabs.take(id)) {
        m_layout->removeWidget(tab);
        tab->deleteLater();
    }
}

void TabStrip::setActiveDocument(int id) {
    clearActiveStates();
    if (Tab* tab = m_docTabs.value(id, nullptr))
        tab->setActive(true);
}

void TabStrip::setActiveHome() {
    clearActiveStates();
    m_home->setActive(true);
}

void TabStrip::showDocsTab() {
    if (!m_docs) {
        m_docs = new Tab(Tab::Kind::Docs, QStringLiteral("book"), tr("Docs"), this);
        m_layout->insertWidget(m_layout->indexOf(m_home) + 1, m_docs); // right after Home
        connect(m_docs, &Tab::clicked, this, &TabStrip::docsSelected);
        connect(m_docs, &Tab::closeClicked, this, &TabStrip::docsCloseRequested);
        refreshIcons();
    }
    setActiveDocs();
}

void TabStrip::closeDocsTab() {
    if (m_docs) {
        m_layout->removeWidget(m_docs);
        m_docs->deleteLater();
        m_docs = nullptr;
    }
}

void TabStrip::setActiveDocs() {
    clearActiveStates();
    if (m_docs)
        m_docs->setActive(true);
}

void TabStrip::clearActiveStates() {
    m_home->setActive(false);
    if (m_docs)
        m_docs->setActive(false);
    for (Tab* tab : std::as_const(m_docTabs))
        tab->setActive(false);
}

void TabStrip::setDocumentTitle(int id, const QString& title) {
    if (Tab* tab = m_docTabs.value(id, nullptr))
        tab->setLabel(title);
}

void TabStrip::setDocumentDirty(int id, bool dirty) {
    if (Tab* tab = m_docTabs.value(id, nullptr))
        tab->setDirty(dirty);
}

QToolButton* TabStrip::makeRightButton(const QString& iconName, const QString& tip) {
    auto* b = new QToolButton(this);
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setObjectName("TabStripButton");
    b->setProperty("iconName", iconName);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(28, 28);
    b->setIconSize(QSize(16, 16));
    b->setAutoRaise(true);
    return b;
}

void TabStrip::refreshIcons() {
    const QColor c = Theme::instance().palette().dim;
    // Reflect the actual mode on the toggle (sun in light, moon in dark).
    if (m_theme)
        m_theme->setProperty("iconName",
                             Theme::instance().mode() == Theme::Mode::Dark ? "moon" : "sun");
    for (QToolButton* b : {m_search, m_theme, m_menu}) {
        if (!b)
            continue;
        b->setIcon(Theme::instance().icon(b->property("iconName").toString(), c));
    }
    update(); // repaint custom-painted tabs with new tokens
}

void TabStrip::paintEvent(QPaintEvent*) {
    // The strip sits on `canvas`; only the active tab paints `surface`.
    QPainter p(this);
    p.fillRect(rect(), Theme::instance().palette().canvas);
}

#include "TabStrip.moc"
