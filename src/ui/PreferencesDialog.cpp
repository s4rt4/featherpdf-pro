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

#include "ui/PreferencesDialog.h"

#include "ui/Theme.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Preferences"));

    const ::Theme::Palette& p = ::Theme::instance().palette();
    const auto css = [](const QColor& c) {
        return c.alpha() == 255 ? c.name(QColor::HexRgb)
                                : QStringLiteral("rgba(%1,%2,%3,%4)")
                                      .arg(c.red())
                                      .arg(c.green())
                                      .arg(c.blue())
                                      .arg(QString::number(c.alphaF(), 'f', 3));
    };
    setStyleSheet(QStringLiteral("QLabel#Head { color:%1; font-size:11px; font-weight:600;"
                                 " letter-spacing:0.4px; }"
                                 "QRadioButton, QLabel { color:%2; }")
                      .arg(css(p.dim), css(p.text)));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 18);
    root->setSpacing(8);

    const auto heading = [&](const QString& text, bool first = false) {
        if (!first)
            root->addSpacing(10);
        auto* h = new QLabel(text.toUpper(), this);
        h->setObjectName(QStringLiteral("Head"));
        root->addWidget(h);
    };
    const auto radio = [&](QButtonGroup* g, int id, const QString& text, bool on) {
        auto* b = new QRadioButton(text, this);
        b->setCursor(Qt::PointingHandCursor);
        g->addButton(b, id);
        if (on)
            b->setChecked(true);
        root->addWidget(b);
    };

    QSettings settings;

    // ── Appearance ──
    heading(tr("Appearance"), /*first=*/true);
    m_theme = new QButtonGroup(this);
    const bool followsSystem = ::Theme::instance().followsSystem();
    const bool dark = ::Theme::instance().mode() == ::Theme::Mode::Dark;
    radio(m_theme, 0, tr("Follow the system theme"), followsSystem);
    radio(m_theme, 1, tr("Light"), !followsSystem && !dark);
    radio(m_theme, 2, tr("Dark"), !followsSystem && dark);

    // ── Default page layout (new documents) ──
    heading(tr("Default page layout"));
    m_layout = new QButtonGroup(this);
    const QString layout = settings.value(QStringLiteral("view/defaultLayout"),
                                          QStringLiteral("continuous")).toString();
    radio(m_layout, 0, tr("Single page"), layout == QLatin1String("single"));
    radio(m_layout, 1, tr("Continuous"), layout != QLatin1String("single") &&
                                             layout != QLatin1String("twoup"));
    radio(m_layout, 2, tr("Two pages"), layout == QLatin1String("twoup"));

    // ── Language ──
    heading(tr("Language"));
    m_language = new QButtonGroup(this);
    const QString language = settings.value(QStringLiteral("app/language"),
                                            QStringLiteral("system")).toString();
    radio(m_language, 0, tr("Follow the system language"),
          language != QLatin1String("en") && language != QLatin1String("id"));
    radio(m_language, 1, tr("English"), language == QLatin1String("en"));
    radio(m_language, 2, tr("Bahasa Indonesia"), language == QLatin1String("id"));
    {
        auto* note = new QLabel(tr("Takes effect after restarting the app."), this);
        note->setObjectName(QStringLiteral("Head"));
        root->addWidget(note);
    }

    // ── Default zoom (new documents) ──
    heading(tr("Default zoom"));
    m_zoom = new QButtonGroup(this);
    const QString zoom = settings.value(QStringLiteral("view/defaultZoom"),
                                        QStringLiteral("fitwidth")).toString();
    radio(m_zoom, 0, tr("Fit width"), zoom != QLatin1String("fitpage") &&
                                          zoom != QLatin1String("actual"));
    radio(m_zoom, 1, tr("Fit page"), zoom == QLatin1String("fitpage"));
    radio(m_zoom, 2, tr("Actual size"), zoom == QLatin1String("actual"));

    root->addSpacing(14);
    auto* buttons = new QDialogButtonBox(this);
    auto* save = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    save->setObjectName(QStringLiteral("Share"));
    save->setCursor(Qt::PointingHandCursor);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    setMinimumWidth(360);
}

PreferencesDialog::Theme PreferencesDialog::theme() const {
    switch (m_theme->checkedId()) {
    case 1: return Theme::Light;
    case 2: return Theme::Dark;
    default: return Theme::System;
    }
}

QString PreferencesDialog::layout() const {
    switch (m_layout->checkedId()) {
    case 0: return QStringLiteral("single");
    case 2: return QStringLiteral("twoup");
    default: return QStringLiteral("continuous");
    }
}

QString PreferencesDialog::zoom() const {
    switch (m_zoom->checkedId()) {
    case 1: return QStringLiteral("fitpage");
    case 2: return QStringLiteral("actual");
    default: return QStringLiteral("fitwidth");
    }
}

QString PreferencesDialog::language() const {
    switch (m_language->checkedId()) {
    case 1: return QStringLiteral("en");
    case 2: return QStringLiteral("id");
    default: return QStringLiteral("system");
    }
}
