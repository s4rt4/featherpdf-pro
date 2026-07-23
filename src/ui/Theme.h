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

#pragma once

#include <QColor>
#include <QIcon>
#include <QObject>
#include <QString>

// The design system, in code. Every color is a token defined exactly once
// (ui-guidelines §2); widgets never hard-code a hex. Theme owns the two token
// palettes (light/dark), generates the global Qt style sheet from them, follows
// the system light/dark preference, and renders the symbolic (lucide) icons the
// mockup uses - tinted to whatever color the caller asks for so they recolor
// correctly across themes.
//
// Token values are taken verbatim from feather-mockup.html / ui-guidelines.md.
class Theme : public QObject {
    Q_OBJECT

public:
    enum class Mode { Light, Dark };

    // The full token set for one mode. Names match the guideline tokens.
    struct Palette {
        QColor canvas;        // app background, inactive-tab strip
        QColor surface;       // active tab, toolbar, panes
        QColor sunken;        // document canvas behind the page
        QColor text;          // primary text
        QColor dim;           // secondary text, inactive tabs
        QColor hairline;      // dividers, pane edges
        QColor accent;        // selection, focus, active tab, primary action
        QColor accentHover;   // accent hover/pressed
        QColor accentTint;    // ~12% accent over surface, for soft selected fills
        QColor warning;       // unsaved-changes dot, caution
        QColor destructive;   // delete, redact
        QColor success;       // confirmation, valid signature
    };

    static Theme& instance();

    // Apply the theme to the whole application (global style sheet + palette).
    // Call once after the QApplication exists. Begins following the system
    // light/dark preference automatically.
    void apply();

    Mode mode() const { return m_mode; }
    bool followsSystem() const { return m_followSystem; }
    const Palette& palette() const { return m_palette; }

    // Flip between light and dark by hand. (The system preference still wins on
    // the next system change; this is the manual affordance from the tab strip.)
    void toggleMode();
    void setMode(Mode mode);
    // Go back to following the system light/dark preference (clears the saved
    // explicit choice).
    void useSystem();

    // A symbolic icon rendered from an embedded SVG (resource path
    // ":/icons/<name>.svg"), tinted to `color`. Cached per (name,color,dpr).
    // When color is invalid, the current `text` token is used.
    QIcon icon(const QString& name, QColor color = QColor()) const;

    // The full-colour brand logo (the prepared feather mark), rendered at `size`
    // logical px without tinting - used on the Home screen, the window icon, and
    // the About box.
    QPixmap brandLogo(int size) const;

    // Bake a tinted copy of symbolic icon `name` to the cache dir and return a
    // filesystem path usable in a QSS `url(...)`. QSS can't tint an SVG, so combo
    // and spin-box arrows reference these baked PNGs. When color is invalid the
    // current `dim` token is used. Cached per (name,color).
    QString symbolicIconPath(const QString& name, QColor color = QColor()) const;

    // A QSS fragment that restores the up/down stepper arrows on a restyled
    // QSpinBox. Once a dialog gives QSpinBox a custom border/background, Qt stops
    // drawing the native arrows; append this to the dialog's style sheet to put
    // themed chevron arrows back. Tinted with the `dim` token.
    QString spinBoxArrowQss() const;

signals:
    // Emitted whenever the active palette changes (system flip or manual toggle)
    // so chrome that paints tokens itself can repaint.
    void changed();

private:
    explicit Theme(QObject* parent = nullptr);

    void rebuild();                       // regenerate QSS + palette, emit changed()
    QString styleSheet() const;           // the global QSS, built from m_palette
    static Palette paletteFor(Mode mode); // the token tables

    Mode m_mode = Mode::Light;
    bool m_followSystem = true;
    Palette m_palette;
};
