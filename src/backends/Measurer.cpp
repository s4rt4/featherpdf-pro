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

#include "backends/Measurer.h"

#include <cmath>

namespace Measurer {

namespace {
// A normalized point ([0,1]) projected onto the page, in PDF points.
QPointF toPoints(const QPointF& norm, const QSizeF& page) {
    return QPointF(norm.x() * page.width(), norm.y() * page.height());
}
} // namespace

double pointsPerUnit(Unit unit) {
    switch (unit) {
    case Unit::Millimeter: return 72.0 / 25.4;
    case Unit::Centimeter: return 72.0 / 2.54;
    case Unit::Inch:       return 72.0;
    }
    return 72.0;
}

double lengthToUnit(double points, Unit unit) {
    return points / pointsPerUnit(unit);
}

double areaToUnit(double squarePoints, Unit unit) {
    const double ppu = pointsPerUnit(unit);
    return squarePoints / (ppu * ppu);
}

double distance(const QVector<QPointF>& normPts, const QSizeF& pageSizePts, Unit unit) {
    if (normPts.size() < 2)
        return 0.0;
    const QPointF a = toPoints(normPts[0], pageSizePts);
    const QPointF b = toPoints(normPts[1], pageSizePts);
    const double d = std::hypot(b.x() - a.x(), b.y() - a.y());
    return lengthToUnit(d, unit);
}

double perimeter(const QVector<QPointF>& normPts, const QSizeF& pageSizePts, Unit unit,
                 bool closed) {
    if (normPts.size() < 2)
        return 0.0;
    double total = 0.0;
    for (int i = 1; i < normPts.size(); ++i) {
        const QPointF a = toPoints(normPts[i - 1], pageSizePts);
        const QPointF b = toPoints(normPts[i], pageSizePts);
        total += std::hypot(b.x() - a.x(), b.y() - a.y());
    }
    if (closed && normPts.size() >= 3) {
        const QPointF a = toPoints(normPts.last(), pageSizePts);
        const QPointF b = toPoints(normPts.first(), pageSizePts);
        total += std::hypot(b.x() - a.x(), b.y() - a.y());
    }
    return lengthToUnit(total, unit);
}

double area(const QVector<QPointF>& normPts, const QSizeF& pageSizePts, Unit unit) {
    if (normPts.size() < 3)
        return 0.0;
    double sum = 0.0; // shoelace, in PDF points²
    for (int i = 0; i < normPts.size(); ++i) {
        const QPointF a = toPoints(normPts[i], pageSizePts);
        const QPointF b = toPoints(normPts[(i + 1) % normPts.size()], pageSizePts);
        sum += a.x() * b.y() - b.x() * a.y();
    }
    return areaToUnit(std::abs(sum) / 2.0, unit);
}

QString unitLabel(Unit unit) {
    switch (unit) {
    case Unit::Millimeter: return QStringLiteral("mm");
    case Unit::Centimeter: return QStringLiteral("cm");
    case Unit::Inch:       return QStringLiteral("in");
    }
    return QStringLiteral("mm");
}

QString areaUnitLabel(Unit unit) {
    return unitLabel(unit) + QStringLiteral("²"); // superscript two
}

QString formatLength(double value, Unit unit) {
    return QStringLiteral("%1 %2").arg(value, 0, 'f', 2).arg(unitLabel(unit));
}

QString formatArea(double value, Unit unit) {
    return QStringLiteral("%1 %2").arg(value, 0, 'f', 2).arg(areaUnitLabel(unit));
}

} // namespace Measurer
