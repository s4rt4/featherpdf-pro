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

#include "ui/AnnotationBar.h"

#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolTip>

AnnotationBar::AnnotationBar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("AnnotationBar"));

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(10, 6, 10, 6);
    row->setSpacing(6);

    // Icon-only tool buttons (the bar is cramped); the tooltip names each tool.
    // Order here defines the toolChanged() index emitted to the page view.
    const struct {
        const char* name;
        const char* icon;
    } toolDefs[] = {
        {QT_TR_NOOP("Highlight"), "highlighter"}, {QT_TR_NOOP("Note"), "sticky-note"},
        {QT_TR_NOOP("Draw"), "pencil"},           {QT_TR_NOOP("Underline"), "underline"},
        {QT_TR_NOOP("Strikeout"), "strikethrough"}, {QT_TR_NOOP("Rectangle"), "square"},
        {QT_TR_NOOP("Line"), "line"},             {QT_TR_NOOP("Arrow"), "arrow-up-right"},
        {QT_TR_NOOP("Text"), "type"}};
    for (const auto& d : toolDefs) {
        auto* b = new QPushButton(this);
        b->setObjectName(QStringLiteral("AnnotTool")); // own style; no #GhostBtn min-width
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tr(d.name));
        b->setAccessibleName(tr(d.name));
        b->setFixedSize(34, 34);
        b->setIconSize(QSize(18, 18));
        m_tools.append(b);
        m_toolIcons << QLatin1String(d.icon);
    }
    m_tools.first()->setChecked(true);

    // Highlight colour swatches.
    m_colors = {QColor(255, 214, 0), QColor(120, 220, 90), QColor(255, 140, 190),
                QColor(110, 190, 255)};
    for (int i = 0; i < m_colors.size(); ++i) {
        auto* sw = new QPushButton(this);
        sw->setObjectName(QStringLiteral("AnnotSwatch"));
        sw->setCheckable(true);
        sw->setCursor(Qt::PointingHandCursor);
        sw->setFixedSize(18, 18);
        sw->setToolTip(tr("Highlight colour"));
        connect(sw, &QPushButton::clicked, this, [this, i] { selectSwatch(i); });
        m_swatches.append(sw);
    }

    // Usage hint folded into a hoverable "i" button so it never crowds the bar.
    m_info = new QPushButton(this);
    m_info->setObjectName(QStringLiteral("AnnotTool"));
    m_info->setCursor(Qt::WhatsThisCursor);
    m_info->setFixedSize(34, 34);
    m_info->setIconSize(QSize(18, 18));
    m_info->setFocusPolicy(Qt::NoFocus);
    const QString hint =
        tr("Pick a tool and draw. Right-click a mark to remove it, then Save to write them in.");
    m_info->setToolTip(hint);
    m_info->setAccessibleName(hint);
    // Hover shows the tooltip; clicking pins it too (for touch / discoverability).
    connect(m_info, &QPushButton::clicked, this, [this, hint] {
        QToolTip::showText(m_info->mapToGlobal(QPoint(0, m_info->height())), hint, m_info);
    });

    m_label = new QLabel(this);
    m_label->setObjectName(QStringLiteral("AnnotLabel"));

    m_save = new QPushButton(tr("Save"), this);
    m_save->setObjectName(QStringLiteral("Share")); // accent-filled primary
    m_save->setToolTip(tr("Save annotations into the PDF"));
    m_save->setCursor(Qt::PointingHandCursor);

    m_clear = new QPushButton(tr("Clear"), this);
    m_clear->setObjectName(QStringLiteral("GhostBtn"));
    m_clear->setCursor(Qt::PointingHandCursor);

    auto* done = new QPushButton(tr("Done"), this);
    done->setObjectName(QStringLiteral("GhostBtn"));
    done->setCursor(Qt::PointingHandCursor);

    for (QPushButton* b : m_tools)
        row->addWidget(b);
    row->addSpacing(8);
    for (QPushButton* sw : m_swatches)
        row->addWidget(sw);
    row->addSpacing(6);
    row->addWidget(m_info);
    row->addWidget(m_label);
    row->addStretch(1);
    row->addWidget(m_clear);
    row->addWidget(m_save);
    row->addWidget(done);

    connect(m_save, &QPushButton::clicked, this, &AnnotationBar::saveRequested);
    connect(m_clear, &QPushButton::clicked, this, &AnnotationBar::clearRequested);
    connect(done, &QPushButton::clicked, this, &AnnotationBar::doneRequested);
    for (int i = 0; i < m_tools.size(); ++i)
        connect(m_tools[i], &QPushButton::clicked, this, [this, i] {
            for (int j = 0; j < m_tools.size(); ++j)
                m_tools[j]->setChecked(j == i);
            applyToolIcons();
            emit toolChanged(i);
        });

    auto applyTheme = [this] {
        const Theme::Palette& p = Theme::instance().palette();
        const auto css = [](const QColor& c) {
            return c.alpha() == 255 ? c.name(QColor::HexRgb)
                                    : QStringLiteral("rgba(%1,%2,%3,%4)")
                                          .arg(c.red())
                                          .arg(c.green())
                                          .arg(c.blue())
                                          .arg(QString::number(c.alphaF(), 'f', 3));
        };
        // Swatches keep their fixed colours; only the checked outline follows text.
        for (int i = 0; i < m_swatches.size(); ++i)
            m_swatches[i]->setStyleSheet(
                QStringLiteral("QPushButton { background:%1; border:1px solid rgba(0,0,0,0.25);"
                               " border-radius:4px; } QPushButton:checked { border:2px solid %2; }")
                    .arg(m_colors[i].name(), css(p.text)));
        setStyleSheet(
            QStringLiteral("#AnnotationBar { background:%1; border-bottom:1px solid %2; }"
                           "#AnnotLabel { color:%3; }"
                           "#AnnotationBar QPushButton#AnnotTool { background:transparent;"
                           " border:1px solid %2; border-radius:8px; padding:0; }"
                           "#AnnotationBar QPushButton#AnnotTool:hover { background:%4; }"
                           "#AnnotationBar QPushButton#AnnotTool:checked { background:%4;"
                           " border:1px solid %5; }")
                .arg(css(p.surface), css(p.hairline), css(p.text), css(p.accentTint),
                     css(p.accent)));
        m_info->setIcon(Theme::instance().icon(QStringLiteral("info"), p.text));
        applyToolIcons();
    };
    applyTheme();
    connect(&Theme::instance(), &Theme::changed, this, applyTheme);

    selectSwatch(0); // default to the first colour
    setCount(0);
}

void AnnotationBar::applyToolIcons() {
    const Theme::Palette& p = Theme::instance().palette();
    for (int i = 0; i < m_tools.size(); ++i) {
        const QColor c = m_tools[i]->isChecked() ? p.accent : p.text;
        m_tools[i]->setIcon(Theme::instance().icon(m_toolIcons[i], c));
    }
}

void AnnotationBar::selectSwatch(int index) {
    for (int i = 0; i < m_swatches.size(); ++i)
        m_swatches[i]->setChecked(i == index);
    if (index >= 0 && index < m_colors.size())
        emit colorChanged(m_colors[index]);
}

void AnnotationBar::setCount(int count) {
    m_label->setText(count == 0 ? QString() : tr("%n annotation(s)", "", count));
    m_save->setEnabled(count > 0);
    m_clear->setEnabled(count > 0);
}
