// Feather PDF — light on the system, full-featured on PDF.
// Copyright (C) 2026 Feather PDF contributors. Licensed under GPLv3 (see LICENSE).
//
// Tests for the measure-tool geometry: distance, polygon perimeter, and area
// (shoelace) on known normalized coordinates over a page of known size, with the
// PDF-point → mm/cm/inch conversions. No GUI - pure maths, run headlessly.

#include "backends/Measurer.h"

#include <QtTest>

class TestMeasurer : public QObject {
    Q_OBJECT

private:
    // A 1×1 inch page: 72 × 72 PDF points. Clean numbers for the conversions.
    const QSizeF kInchPage{72.0, 72.0};

private slots:
    void distanceAcrossPage() {
        const QVector<QPointF> pts{{0.0, 0.0}, {1.0, 0.0}}; // the full page width
        // Full width = 72 pt = 1 inch = 25.4 mm = 2.54 cm.
        QCOMPARE(Measurer::distance(pts, kInchPage, Measurer::Unit::Inch), 1.0);
        QVERIFY(qFuzzyCompare(Measurer::distance(pts, kInchPage, Measurer::Unit::Millimeter), 25.4));
        QVERIFY(qFuzzyCompare(Measurer::distance(pts, kInchPage, Measurer::Unit::Centimeter), 2.54));
    }

    void diagonalDistance() {
        const QVector<QPointF> pts{{0.0, 0.0}, {1.0, 1.0}}; // corner to corner
        // sqrt(72² + 72²) pt = 72·√2 pt = √2 inch.
        QVERIFY(qFuzzyCompare(Measurer::distance(pts, kInchPage, Measurer::Unit::Inch),
                              std::sqrt(2.0)));
    }

    void fewerThanTwoPointsIsZero() {
        QCOMPARE(Measurer::distance({{0.5, 0.5}}, kInchPage, Measurer::Unit::Inch), 0.0);
        QCOMPARE(Measurer::distance({}, kInchPage, Measurer::Unit::Inch), 0.0);
    }

    void perimeterOfUnitSquare() {
        const QVector<QPointF> sq{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
        // Closed: four edges of one inch each = 4 in.
        QVERIFY(qFuzzyCompare(Measurer::perimeter(sq, kInchPage, Measurer::Unit::Inch, true), 4.0));
        // Open polyline: three edges = 3 in.
        QVERIFY(qFuzzyCompare(Measurer::perimeter(sq, kInchPage, Measurer::Unit::Inch, false), 3.0));
    }

    void areaOfUnitSquare() {
        const QVector<QPointF> sq{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
        QVERIFY(qFuzzyCompare(Measurer::area(sq, kInchPage, Measurer::Unit::Inch), 1.0));
        // 1 in² = 25.4² mm² = 645.16 mm²; = 2.54² cm² = 6.4516 cm².
        QVERIFY(qFuzzyCompare(Measurer::area(sq, kInchPage, Measurer::Unit::Millimeter), 645.16));
        QVERIFY(qFuzzyCompare(Measurer::area(sq, kInchPage, Measurer::Unit::Centimeter), 6.4516));
    }

    void areaIgnoresWindingDirection() {
        const QVector<QPointF> cw{{0.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}};
        QVERIFY(qFuzzyCompare(Measurer::area(cw, kInchPage, Measurer::Unit::Inch), 1.0));
    }

    void areaOfTriangle() {
        // Right triangle filling half the page: base 1 in, height 1 in → 0.5 in².
        const QVector<QPointF> tri{{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};
        QVERIFY(qFuzzyCompare(Measurer::area(tri, kInchPage, Measurer::Unit::Inch), 0.5));
    }

    void areaNeedsThreePoints() {
        QCOMPARE(Measurer::area({{0.0, 0.0}, {1.0, 1.0}}, kInchPage, Measurer::Unit::Inch), 0.0);
    }

    void nonSquarePageScalesPerAxis() {
        // A 2×1 inch page (144 × 72 pt): full width spans 2 inches.
        const QSizeF page{144.0, 72.0};
        const QVector<QPointF> pts{{0.0, 0.0}, {1.0, 0.0}};
        QVERIFY(qFuzzyCompare(Measurer::distance(pts, page, Measurer::Unit::Inch), 2.0));
        // Full-page rectangle area = 2 in².
        const QVector<QPointF> sq{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
        QVERIFY(qFuzzyCompare(Measurer::area(sq, page, Measurer::Unit::Inch), 2.0));
    }

    void formatting() {
        QCOMPARE(Measurer::formatLength(25.4, Measurer::Unit::Millimeter), QStringLiteral("25.40 mm"));
        QCOMPARE(Measurer::formatArea(6.4516, Measurer::Unit::Centimeter),
                 QString::fromUtf8("6.45 cm\xc2\xb2"));
    }
};

QTEST_GUILESS_MAIN(TestMeasurer)
#include "test_measurer.moc"
