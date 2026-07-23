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
#include <QRectF>
#include <QString>

// Authors dynamic rubber-stamp (/Stamp) annotations with QPDF: a rounded,
// bordered rectangle plus a bold coloured caption, drawn into the annotation's
// /AP appearance stream so every viewer renders it identically. Each preset has
// its own colour, and the caption can carry an optional date and/or name add-on.
class StampLibrary {
public:
    enum class Preset { Approved, Draft, Confidential, ForReview };

    // The caption for a preset plus optional add-ons. `date` and `name` are
    // appended (when non-empty) after a dash, e.g. "APPROVED - 2026-06-27"
    // or "CONFIDENTIAL - Vinvan".
    static QString caption(Preset preset, const QString& date, const QString& name);

    // The accent colour for a preset (green/amber/red/blue).
    static QColor color(Preset preset);

    // Stamp `normRect` (displayed-page fractions, top-left origin) on 0-based
    // `page` with `preset` and the optional `date`/`name` add-ons. Produce-then-
    // open style: writes `outputPath`. Returns true on success; fills *error.
    static bool addStamp(const QString& inputPath, const QString& outputPath, int page,
                         const QRectF& normRect, Preset preset, const QString& date,
                         const QString& name, QString* error);
};
