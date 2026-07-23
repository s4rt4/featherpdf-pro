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

#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QVector>

// Pure geometry for the measure tools. Points arrive as fractions of the page
// ([0,1] in both axes); the page size in PDF points (1 pt = 1/72 inch) sets the
// real-world scale. Everything is unit-agnostic until the final conversion, so
// the same coordinates yield mm, cm, or inches with no GUI involved (testable
// headlessly). The maths: length via Euclidean distance, area via the shoelace
// formula, both computed in points then converted by the unit's scale factor.
namespace Measurer {

enum class Unit { Millimeter, Centimeter, Inch };

// PDF points per one unit (1 inch = 72 pt; 1 mm = 72/25.4 pt).
double pointsPerUnit(Unit unit);

// Convert a length / an area given in PDF points to the chosen unit.
double lengthToUnit(double points, Unit unit);
double areaToUnit(double squarePoints, Unit unit);

// Distance between the first two normalized points (0 if fewer than two).
double distance(const QVector<QPointF>& normPts, const QSizeF& pageSizePts, Unit unit);

// Total polyline length. When `closed`, the segment from the last point back to
// the first is included (the perimeter of the polygon).
double perimeter(const QVector<QPointF>& normPts, const QSizeF& pageSizePts, Unit unit,
                 bool closed = true);

// Polygon area (shoelace), always non-negative (0 if fewer than three points).
double area(const QVector<QPointF>& normPts, const QSizeF& pageSizePts, Unit unit);

// Short labels for overlay text: "mm" / "cm" / "in" and their squared forms.
QString unitLabel(Unit unit);
QString areaUnitLabel(Unit unit);

// A formatted length / area string with two decimals, e.g. "25.40 mm".
QString formatLength(double value, Unit unit);
QString formatArea(double value, Unit unit);

} // namespace Measurer
